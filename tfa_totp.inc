<?php

/**
 * @file classes for tfa_totp
 */

/**
 * Class TfaTotp
 */
class TfaTotp extends TfaBasePlugin implements TfaValidationPluginInterface {

  /**
   * @var PHPGangsta_GoogleAuthenticator
   */
  protected $ga;

  /**
   * @var int
   */
  protected $timeSkew;

  /**
   * @copydoc TfaBasePlugin::__construct()
   */
  public function __construct(array $context) {
    parent::__construct($context);
    $this->ga = new PHPGangsta_GoogleAuthenticator();
    // Allow codes within tolerance range of 2 * 30 second units.
    $this->timeSkew = variable_get('tfa_totp_time_skew', 2);
    $this->encryptionKey = variable_get('tfa_totp_secret_key', ''); // Set in settings.php.
  }

  /**
   * @copydoc TfaBasePlugin::ready()
   */
  public function ready() {
    return ($this->getSeed() !== FALSE);
  }

  /**
   * @copydoc TfaBasePlugin::getForm()
   */
  public function getForm(array $form, array &$form_state) {
    $form['code'] = array(
      '#type' => 'textfield',
      '#title' => t('Verification code'),
      '#description' => t('Verification code is application generated and six digits long.'),
      '#required' => TRUE,
    );
    $form['actions']['login'] = array(
      '#type' => 'submit',
      '#value' => t('Verify'),
    );

    return $form;
  }

  /**
   * @copydoc TfaBasePlugin::validateForm()
   */
  public function validateForm(array $form, array &$form_state) {
    if (!$this->validate($form_state['values']['code'])) {
      $this->errorMessages['code'] = t('Invalid code. Please try again.');
      return FALSE;
    }
    else {
      return TRUE;
    }
  }

  /**
   * @copydoc TfaBasePlugin::validate()
   */
  protected function validate($code) {
    // Get OTP seed.
    $seed = $this->getSeed();
    $this->valid = ($seed && $this->ga->verifyCode($seed, $code, $this->timeSkew));
    return $this->valid;
  }

  /**
   * Get seed for this account.
   *
   * @return string Decrypted account OTP seed or FALSE if none exists.
   */
  protected function getSeed() {
    // Lookup seed for account and decrypt.
    $result = db_query("SELECT seed FROM {tfa_totp_seed} WHERE uid = :uid", array(':uid' => $this->context['uid']))->fetchAssoc();
    if (!empty($result)) {
      $encrypted = base64_decode($result['seed']);
      $seed = $this->decrypt($encrypted);
      if (!empty($seed)) {
        return $seed;
      }
    }
    return FALSE;
  }
}

/**
 * Class TfaTotpSetup
 */
class TfaTotpSetup extends TfaTotp implements TfaSetupPluginInterface {

  /**
   * @var string Un-encrypted seed.
   */
  protected $seed;

  /**
   * @var string
   */
  protected $namePrefix;

  /**
   * @copydoc TfaBasePlugin::__construct()
   */
  public function __construct(array $context) {
    parent::__construct($context);
    // Generate seed.
    $this->seed = $this->createSeed();
    $this->namePrefix = variable_get('tfa_totp_name_prefix', variable_get('site_name', 'Drupal'));
  }

  /**
   * @copydoc TfaSetupPluginInterface::getSetupForm()
   */
  public function getSetupForm(array $form, array &$form_state) {
    // Optional show text seed.
    $form['show_seed'] = array(
      '#type' => 'checkbox',
      '#title' => t('Show text code'),
    );
    $form['seed'] = array(
      '#type' => 'textfield',
      '#value' => $this->seed,
      '#disabled' => TRUE,
      '#states' => array(
        'visible' => array(
          ':input[name="show_seed"]' => array('checked' => TRUE)
        )
      ),
    );
    // QR image of seed. @todo add alt
    $form['qr_image'] = array(
      '#markup' => '<img src="' . $this->getQrCodeUrl($this->seed) .'">',
    );
    // Include code entry form.
    $form = $this->getForm($form, $form_state);
    return $form;
  }

  /**
   * @copydoc TfaSetupPluginInterface::validateSetupForm()
   */
  public function validateSetupForm(array $form, array &$form_state) {
    if (!$this->validate($form_state['values']['code'])) {
      $this->errorMessages['code'] = t('Invalid code. Please try again.');
      return FALSE;
    }
    else {
      return TRUE;
    }
  }

  /**
   * @copydoc TfaBasePlugin::validate()
   */
  protected function validate($code) {
    return $this->ga->verifyCode($this->seed, $code, $this->timeSkew);
  }

  /**
   * @copydoc TfaSetupPluginInterface::submitSetupForm()
   */
  public function submitSetupForm(array $form, array &$form_state) {
    // Write seed for user.
    $this->storeSeed($this->seed);
    return TRUE;
  }

  /**
   * Get a URL to a Google Chart QR image for a seed.
   *
   * @param string $seed
   * @return string URL
   */
  protected function getQrCodeUrl($seed) {
    // Note, this URL is over https but does leak the seed and account
    // email address to Google. @todo consider generating the QR image locally.
    return $this->ga->getQRCodeGoogleUrl($this->accountName(), $seed);
  }

  /**
   * Create OTP seed for account.
   *
   * @return string Seed.
   */
  protected function createSeed() {
    return $this->ga->createSecret();
  }

  /**
   * Save seed for account.
   *
   * @param string $seed Seed.
   */
  protected function storeSeed($seed) {
    // Encrypt seed for storage.
    $encrypted = $this->encrypt($seed);
    // Data is binary so store base64 encoded.
    $record = array(
      'uid' => $this->context['uid'],
      'seed' => base64_encode($encrypted),
      'created' => REQUEST_TIME
    );

    $existing = $this->getSeed();
    if (!empty($existing)) {
      // Update existing seed.
      drupal_write_record('tfa_totp_seed', $record, 'uid');
    }
    else {
      drupal_write_record('tfa_totp_seed', $record);
    }
  }

  /**
   * Get account name for QR image.
   *
   * @return string URL encoded string.
   */
  protected function accountName() {
    $account = user_load($this->context['uid']);
    return urlencode($this->namePrefix . '-' . $account->mail);
  }

}
