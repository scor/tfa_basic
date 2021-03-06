<?php

/**
 * @file
 * Classes for TFA basic plugin.
 */

/**
 * Class TfaTrustedDevice
 */
class TfaTrustedDevice extends TfaBasePlugin implements TfaLoginPluginInterface {

  /**
   * @var bool
   */
  protected $trustDevice;

  /**
   * @var string
   */
  protected $cookieName;

  /**
   * @var string
   */
  protected $domain;

  /**
   * Expiration in seconds.
   *
   * Default is 30 days. (3600 * 24 * 30)
   *
   * @var int
   */
  protected $expiration;

  /**
   * {@inheritdoc}
   */
  public function __construct(array $context) {
    parent::__construct($context);

    $this->cookieName = variable_get('tfa_basic_cookie_name', 'TD');
    $this->domain = variable_get('tfa_basic_cookie_domain', '');

    // Expiration defaults to 30 days.
    $this->expiration = variable_get('tfa_basic_trust_cookie_expiration', 3600 * 24 * 30);
  }

  /**
   * @return bool
   */
  public function loginAllowed() {
    if (isset($_COOKIE[$this->cookieName]) && ($did = $this->trustedDevice($_COOKIE[$this->cookieName])) !== FALSE) {
      $this->setUsed($did);

      return TRUE;
    }

    return FALSE;
  }

  /**
   * {@inheritdoc}
   */
  public function getForm(array $form, array &$form_state) {
    $form['trust_device'] = array(
      '#type' => 'checkbox',
      '#title' => t('Remember this browser?'),
      '#description' => t('Not recommended if you are on a public or shared computer.'),
    );

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function submitForm(array $form, array &$form_state) {
    if (isset($form_state['values']['trust_device']) && $form_state['values']['trust_device']) {
      $this->trustDevice = TRUE;
    }
    else {
      $this->trustDevice = FALSE;
    }
  }

  public function finalize() {
    if ($this->trustDevice) {
      $name = $this->getAgent();
      $this->setTrusted($this->generateDeviceId(), $name);
    }
  }

  /**
   * Generate a random value to identify the device.
   *
   * @return string
   */
  protected function generateDeviceId() {
    $id = base64_encode(drupal_random_bytes(32));

    return strtr($id, array(
      '+' => '-',
      '/' => '_',
      '=' => '',
    ));
  }

  /**
   * Store device value and issue cookie for user.
   *
   * @param string $value
   *
   * @param string $name
   */
  protected function setTrusted($value, $name = '') {
    // Store id for account.
    $record = array(
      'uid' => $this->context['uid'],
      'value' => $value,
      'created' => $this->requestTime,
      'ip' => ip_address(),
      'name' => $name,
    );
    drupal_write_record('tfa_trusted_device', $record);
    // Issue cookie with ID.
    $cookie_secure = ini_get('session.cookie_secure');
    $expiration = $this->requestTime + $this->expiration;
    setcookie($this->cookieName, $value, $expiration, '/', $this->domain, (empty($cookie_secure) ? FALSE : TRUE), TRUE);
    $name = empty($name) ? $this->getAgent() : $name;
    watchdog(
      'tfa_basic',
      'Set trusted browser for user UID !uid, browser @name',
      array(
        '!uid' => $this->context['uid'],
        '@name' => $name,
      ),
      WATCHDOG_INFO
    );
  }

  /**
   * Updated device last used time.
   *
   * @param int $did
   *   Internal device ID to update.
   */
  protected function setUsed($did) {
    $record = array(
      'did' => $did,
      'last_used' => $this->requestTime,
    );
    drupal_write_record('tfa_trusted_device', $record, 'did');
  }

  /**
   * Check if device value matches user's saved device.
   *
   * @param string $value
   *
   * @return int|FALSE
   *   Device ID if trusted or else FALSE.
   */
  protected function trustedDevice($value) {
    // Check if $id has been saved for this user.
    return db_result(db_query("SELECT did FROM {tfa_trusted_device} WHERE value = '%s' AND uid = %d", $value, $this->context['uid']));
  }

  /**
   * Delete users trusted devices.
   *
   * @param int $did
   *   Optional trusted device id to delete.
   *
   * @return int
   *   Number of deleted rows.
   */
  protected function deleteTrusted($did = NULL) {
    $query_where = array('uid = %d');
    $query_args = array($this->context['uid']);

    if (is_int($did)) {
      $query_where[] = 'did = %d';
      $query_args[] = $did;
    }

    $query = 'DELETE FROM {tfa_trusted_device} WHERE ' . implode(' AND ', $query_where);
    db_query($query, $query_args);

    return db_affected_rows();
  }

  /**
   * Get simplified device name from user agent.
   *
   * @param string $name
   *   Default name.
   *
   * @return string
   *   Name of the browser.
   */
  protected function getAgent($name = '') {
    if (isset($_SERVER['HTTP_USER_AGENT'])) {
      // Match popular user agents.
      $agent = $_SERVER['HTTP_USER_AGENT'];
      if (preg_match("/like\sGecko\)\sChrome\//", $agent)) {
        $name = 'Chrome';
      }
      elseif (strpos($_SERVER['HTTP_USER_AGENT'], 'Firefox') !== FALSE) {
        $name = 'Firefox';
      }
      elseif (strpos($_SERVER['HTTP_USER_AGENT'], 'MSIE') !== FALSE) {
        $name = 'Internet Explorer';
      }
      elseif (strpos($_SERVER['HTTP_USER_AGENT'], 'Safari') !== FALSE) {
        $name = 'Safari';
      }
      else {
        // Otherwise filter agent and truncate to column size.
        $name = substr($agent, 0, 255);
      }
    }

    return $name;
  }

}

/**
 * Class TfaDeviceTrustSetup
 */
class TfaTrustedDeviceSetup extends TfaTrustedDevice implements TfaSetupPluginInterface {

  /**
   * {@inheritdoc}
   */
  public function getSetupForm(array $form, array &$form_state) {
    $existing = $this->getTrustedDevices();
    $time = variable_get('tfa_basic_trust_cookie_expiration', 3600 * 24 * 30) / (3600 * 24);
    $form['info'] = array(
      '#type' => 'markup',
      '#value' => '<p>' . t("Trusted browsers are a method for simplifying log in by avoiding TFA code entry for a set amount of time, !time days from marking a browser as trusted. After !time days, to log in you'll need to enter a TFA code with your username and password during which you can again mark the browser as trusted.", array('!time' => $time)) . '</p>',
    );

    // Present option to trust this browser if its not currently trusted.
    if (isset($_COOKIE[$this->cookieName]) && ($device_did = $this->trustedDevice($_COOKIE[$this->cookieName])) !== FALSE) {
      $current_trusted_device_did = $device_did;
    }
    else {
      $current_trusted_device_did = FALSE;
      $form['trust'] = array(
        '#type' => 'checkbox',
        '#title' => t('Trust this browser?'),
        '#default_value' => empty($existing) ? 1 : 0,
      );
      // Optional field to name this device.
      $form['name'] = array(
        '#type' => 'textfield',
        '#title' => t('Name this browser'),
        '#maxlength' => 255,
        '#description' => t('Optionally, name the browser on your device (e.g. "home firefox" or "office desktop windows"). Your current browser user agent is %browser', array('%browser' => $_SERVER['HTTP_USER_AGENT'])),
        '#default_value' => $this->getAgent(),
        '#states' => array(
          'visible' => array(
            ':input[name="trust"]' => array('checked' => TRUE),
          ),
        ),
      );
    }

    if (!empty($existing)) {
      $form['existing'] = array(
        '#type' => 'fieldset',
        '#title' => t('Existing browsers'),
        '#description' => t('Leave checked to keep these browsers in your trusted log in list.'),
        '#tree' => TRUE,
      );

      foreach ($existing as $device) {
        $vars = array(
          '!set' => format_date($device['created']),
          '!time' => format_date($device['last_used']),
        );

        if ($current_trusted_device_did == $device['did']) {
          $name = '<strong>' . t('@name (current browser)', array('@name' => $device['name'])) . '</strong>';
        }
        else {
          $name = check_plain($device['name']);
        }

        if (empty($device['last_used'])) {
          $message = t('Marked trusted !set', $vars);
        }
        else {
          $message = t('Marked trusted !set, last used for log in !time', $vars);
        }

        $form['existing']['trusted_device_' . $device['did']] = array(
          '#type' => 'checkbox',
          '#title' => $name,
          '#description' => $message,
          '#default_value' => 1,
        );
      }
    }

    $form['actions']['save'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function validateSetupForm(array $form, array &$form_state) {
    // Do nothing, no validation required.
    return TRUE;
  }

  /**
   * {@inheritdoc}
   */
  public function submitSetupForm(array $form, array &$form_state) {
    if (isset($form_state['values']['existing'])) {
      $count = 0;
      foreach ($form_state['values']['existing'] as $element => $value) {
        $id = str_replace('trusted_device_', '', $element);
        if (!$value) {
          $this->deleteTrusted($id);
          $count++;
        }
      }

      if ($count) {
        watchdog('tfa_basic', 'Removed !num TFA trusted browsers during trusted browser setup', array('!num' => $count), WATCHDOG_INFO);
      }
    }

    if (!empty($form_state['values']['trust']) && $form_state['values']['trust']) {
      $name = '';
      if (!empty($form_state['values']['name'])) {
        $name = $form_state['values']['name'];
      }
      elseif (isset($_SERVER['HTTP_USER_AGENT'])) {
        $name = $this->getAgent();
      }

      $this->setTrusted($this->generateDeviceId(), $name);
    }

    return TRUE;
  }

  /**
   * Get list of trusted devices.
   */
  public function getTrustedDevices() {
    $trusted_devices = array();
    $result = db_query('SELECT did, name, created, ip, last_used FROM {tfa_trusted_device} WHERE uid = %d', $this->context['uid']);
    while (($row = db_fetch_array($result))) {
      $trusted_devices[$row['did']] = $row;
    }

    return $trusted_devices;
  }

  /**
   * Delete a trusted device by its ID.
   *
   * @param int $id
   *
   * @return int
   *   Number of deleted rows.
   */
  public function deleteTrustedId($id) {
    return $this->deleteTrusted($id);
  }

  /**
   * Delete all trusted devices.
   *
   * @return int
   *   Number of deleted rows.
   */
  public function deleteTrustedDevices() {
    return $this->deleteTrusted();
  }
}
