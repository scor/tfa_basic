<?php

/**
 * @file classes for TFA basic plugin
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
   * @var string
   */
  protected $expiration;

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
   * @copydoc TfaValidationPluginInterface::getForm()
   */
  public function getForm(array $form, array &$form_state) {
    $form['trust_device'] = array(
      '#type' => 'checkbox',
      '#title' => t('Remember this browser?'),
      '#description' => t('Not recommended if you are on a public computer.'),
    );
    return $form;
  }

  /**
   * @copydoc TfaBasePlugin::submitForm()
   */
  public function submitForm(array $form, array &$form_state) {
    if (isset($form_state['values']['trust_device']) && $form_state['values']['trust_device']) {
      $this->trustDevice = TRUE;
    }
    else {
      $this->trustDevice = FALSE;
    }
  }

  /**
   *
   */
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
    return strtr($id, array('+' => '-', '/' => '_', '=' => ''));
  }

  /**
   * Store device value and issue cookie for user.
   *
   * @param string $value
   * @param string $name
   */
  protected function setTrusted($value, $name = '') {
    // Store id for account.
    $record = array(
      'uid' => $this->context['uid'],
      'value' => $value,
      'created' => REQUEST_TIME,
      'ip' => ip_address(),
      'name' => $name,
    );
    drupal_write_record('tfa_trusted_device', $record);
    // Issue cookie with ID.
    $cookie_secure = ini_get('session.cookie_secure');
    $expiration = REQUEST_TIME + $this->expiration;
    setcookie($this->cookieName, $value, $expiration, '/', $this->domain, (empty($cookie_secure) ? FALSE : TRUE), TRUE);
    watchdog('TFA', 'Set trusted browser for user !uid', array('!uid' => $this->context['uid']), WATCHDOG_INFO);
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
      'last_used' => REQUEST_TIME,
    );
    drupal_write_record('tfa_trusted_device', $record, 'did');
  }

  /**
   * Check if device value matches user's saved device.
   *
   * @param string $value
   * @return int|FALSE
   *   Device ID if trusted or else FALSE.
   */
  protected function trustedDevice($value) {
    // Check if $id has been saved for this user.
    $result = db_query("SELECT did FROM {tfa_trusted_device} WHERE value = :value AND uid = :uid", array(':value' => $value, ':uid' => $this->context['uid']))->fetchAssoc();
    if (!empty($result)) {
      return $result['did'];
    }
    return FALSE;
  }

  /**
   * Delete users trusted devices.
   *
   * @param int $did
   *   Optional trusted device id to delete.
   *
   * @return int
   */
  protected function deleteTrusted($did = NULL) {
    $query = db_delete('tfa_trusted_device')
      ->condition('uid', $this->context['uid']);
    if (is_int($did)) {
      $query->condition('did', $did);
    }

    return $query->execute();
  }

  /**
   * Get simplified device name from user agent.
   *
   * @param string $name Default name.
   *
   * @return string
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

  public function __construct(array $context) {
    parent::__construct($context);
  }

  /**
   * @copydoc TfaSetupPluginInterface::getSetupForm()
   */
  public function getSetupForm(array $form, array &$form_state) {
    $form['remove'] = array(
      '#type' => 'checkbox',
      '#title' => t('Remove existing'),
      '#description' => t('Remove any existing trusted devices'),
      '#default_value' => 1,
    );
    // Optional field to name this device.
    $form['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Name this browser'),
      '#maxlength' => 255,
      '#description' => t('Name the browser on your device (e.g. "home firefox" or "office desktop windows"). Leaving blank will use your current user agent.'),
    );
    if (!empty($_SERVER['HTTP_USER_AGENT'])) {
      $form['agent'] = array(
        '#type' => 'markup',
        '#markup' => '<p>' . t('Current user agent: @browser', array('@browser' => $_SERVER['HTTP_USER_AGENT'])) . '</p>',
      );
    }
    $form['actions']['save'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );

    return $form;
  }

  /**
   * @copydoc TfaSetupPluginInterface::validateSetupForm()
   */
  public function validateSetupForm(array $form, array &$form_state) {
    return TRUE; // Do nothing, no validation required.
  }

  /**
   * @copydoc TfaSetupPluginInterface::submitSetupForm()
   */
  public function submitSetupForm(array $form, array &$form_state) {
    if (!empty($form_state['values']['remove'])) {
      $this->deleteTrustedDevices();
    }

    if (!empty($form_state['values']['trust_device']) && $form_state['values']['trust_device']) {
      // Delete any currently trusted devices.
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
   * Delete a trusted device by its ID.
   *
   * @param int $id
   * @return int
   */
  public function deleteTrustedId($id) {
    return $this->deleteTrusted($id);
  }

  /**
   * Delete all trusted devices.
   *
   * @return int
   */
  public function deleteTrustedDevices() {
    return $this->deleteTrusted();
  }
}
