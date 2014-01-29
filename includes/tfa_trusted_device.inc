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
    if (isset($_COOKIE[$this->cookieName]) && $this->trustedDevice($_COOKIE[$this->cookieName])) {
      return TRUE;
    }
    return FALSE;
  }

  /**
   * @param $form
   * @return array
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
   * @param array $form_state
   * @return null
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
      // Generate random device value ID.
      $id = base64_encode(drupal_random_bytes(32));
      $id = strtr($id, array('+' => '-', '/' => '_', '=' => ''));
      $this->setTrusted($id);
    }
  }

  /**
   * Store device value and issue cookie for user.
   *
   * @param string $value
   */
  protected function setTrusted($value) {
    // Store id for account.
    $record = array(
      'uid' => $this->context['uid'],
      'value' => $value,
      'created' => REQUEST_TIME,
      'ip' => ip_address(),
    );
    drupal_write_record('tfa_trusted_device', $record);
    // Issue cookie with ID.
    $cookie_secure = ini_get('session.cookie_secure');
    $expiration = REQUEST_TIME + $this->expiration;
    setcookie($this->cookieName, $value, $expiration, '/', $this->domain, (empty($cookie_secure) ? FALSE : TRUE), TRUE);
  }

  /**
   * Check if device value matches user's saved device.
   *
   * @param string $value
   * @return bool
   */
  protected function trustedDevice($value) {
    // Check if $id has been saved for this user.
    $result = db_query("SELECT uid FROM {tfa_trusted_device} WHERE value = :value AND uid = :uid", array(':value' => $value, ':uid' => $this->context['uid']))->fetchAssoc();
    if (!empty($result)) {
      return TRUE;
    }
    return FALSE;
  }

}
