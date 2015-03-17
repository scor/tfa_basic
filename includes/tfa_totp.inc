<?php

/**
 * @file
 * Classes for tfa_totp.
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
   * {@inheritdoc}
   */
  public function __construct(array $context) {
    parent::__construct($context);
    $this->ga = new PHPGangsta_GoogleAuthenticator();
    // Allow codes within tolerance range of 2 * 30 second units.
    $this->timeSkew = variable_get('tfa_basic_time_skew', 2);
    // Recommended: set variable tfa_totp_secret_key in settings.php.
    $this->encryptionKey = variable_get('tfa_basic_secret_key', drupal_get_private_key());
  }

  /**
   * {@inheritdoc}
   */
  public function ready() {
    return ($this->getSeed() !== FALSE);
  }

  /**
   * {@inheritdoc}
   */
  public function getForm(array $form, array &$form_state) {
    $form['code'] = array(
      '#type' => 'textfield',
      '#title' => t('Application verification code'),
      '#description' => t('Verification code is application generated and !length digits long.', array('!length' => $this->codeLength)),
      '#required' => TRUE,
      '#attributes' => array('autocomplete' => 'off'),
    );

    $form['actions']['login'] = array(
      '#type' => 'submit',
      '#value' => t('Verify'),
    );

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function validateForm(array $form, array &$form_state) {
    if (!$this->validate($form_state['values']['code'])) {
      $this->errorMessages['code'] = t('Invalid application code. Please try again.');

      return FALSE;
    }
    else {
      return TRUE;
    }
  }

  /**
   * {@inheritdoc}
   */
  protected function validate($code) {
    // Get OTP seed.
    $seed = $this->getSeed();
    $this->isValid = ($seed && $this->ga->verifyCode($seed, $code, $this->timeSkew));

    return $this->isValid;
  }

  /**
   * Get seed for this account.
   *
   * @return string
   *   Decrypted account OTP seed or FALSE if none exists.
   */
  protected function getSeed() {
    // Lookup seed for account and decrypt.
    $result = db_query('SELECT seed FROM {tfa_totp_seed} WHERE uid = %d', $this->context['uid']);
    if (($row = db_fetch_object($result))) {
      $encrypted = base64_decode($row->seed);
      $seed = $this->decrypt($encrypted);
      if (!empty($seed)) {
        return $seed;
      }
    }

    return FALSE;
  }

  /**
   * Delete users seeds.
   *
   * @return int
   */
  public function deleteSeed() {
    db_query('DELETE FROM {tfa_totp_seed} WHERE uid = %d', $this->context['uid']);

    return db_affected_rows();
  }
}

/**
 * Class TfaTotpSetup
 */
class TfaTotpSetup extends TfaTotp implements TfaSetupPluginInterface {

  /**
   * Un-encrypted seed.
   *
   * @var string
   */
  protected $seed;

  /**
   * @var string
   */
  protected $namePrefix;

  /**
   * {@inheritdoc}
   */
  public function __construct(array $context) {
    parent::__construct($context);

    // Generate seed.
    $this->seed = $this->createSeed();
    $this->namePrefix = variable_get('tfa_basic_name_prefix', variable_get('site_name', 'Drupal'));
  }

  /**
   * {@inheritdoc}
   */
  public function getSetupForm(array $form, array &$form_state) {
    $options = array(
      'external' => TRUE,
      'attributes' => array(
        'target' => '_blank',
      ),
    );

    $items = array(
      l('Google Authenticator (Android/iPhone/BlackBerry)', 'https://support.google.com/accounts/answer/1066447?hl=en', $options),
      l('Authy (Android/iPhone)', 'https://www.authy.com/thefuture#install-now', $options),
      l('Authenticator (Windows Phone)', 'http://www.windowsphone.com/en-us/store/app/authenticator/021dd79f-0598-e011-986b-78e7d1fa76f8', $options),
      l('FreeOTP (Android)', 'https://play.google.com/store/apps/details?id=org.fedorahosted.freeotp', $options),
      l('GAuth Authenticator (desktop)', 'https://github.com/gbraad/html5-google-authenticator', $options)
    );

    $form['apps'] = array(
      '#type' => 'markup',
      '#value' => theme(
        'item_list',
        $items,
        t('Install authentication code application on your mobile or desktop device:')
      ),
    );

    $form['info'] = array(
      '#type' => 'markup',
      '#value' => t('<p>The two-factor authentication application will be used during this setup and for generating codes during regular authentication. If the application supports it, scan the QR code below to get the setup code otherwise you can manually enter the text code.</p>'),
    );

    $form['seed'] = array(
      '#type' => 'textfield',
      '#value' => $this->seed,
      '#disabled' => TRUE,
      '#description' => t('Enter this code into your two-factor authentication app or scan the QR code below.'),
    );

    // QR image of seed.
    // @todo Use libraries module.
    $path = drupal_get_path('module', 'tfa_basic');
    if (file_exists("$path/includes/qrcodejs/qrcode.min.js")) {
      $qrcode_wrapper_id = form_clean_id('tfa-qrcode');
      $qrcode_text = 'otpauth://totp/' . $this->accountName() . '?secret=' . $this->seed;

      drupal_add_js("$path/includes/qrcodejs/qrcode.min.js");
      drupal_add_js("$path/js/tfa_basic.qrcode.js");
      drupal_add_js(
        array(
          'tfaBasic' => array(
            'qrCodes' => array(
              $qrcode_wrapper_id => array('text' => $qrcode_text),
            ),
          ),
        ),
        'setting'
      );

      $form['qr_image_wrapper']['qr_image'] = array(
        '#type' => 'markup',
        '#value' => sprintf('<div class="tfa-qrcode" id="%s"></div>', $qrcode_wrapper_id),
      );

      $form['qr_image_wrapper']['qr_image']['#attached']['library'][] = array('tfa_basic', 'qrcodejs');
      $form['qr_image_wrapper']['qr_image']['#attached']['js'][] = array(
        'data' => 'jQuery(document).ready(function () { new QRCode(document.getElementById("tfa-qrcode"), "' . $qrcode_text . '");});',
        'type' => 'inline',
        'scope' => 'footer',
        'weight' => 5,
      );
    }
    else {
      $attributes = drupal_attributes(array(
        'src' => $this->getQrCodeUrl($this->seed),
        'alt' => t('QR code for TFA setup'),
      ));
      $form['qr_image'] = array(
        '#type' => 'markup',
        '#value' => "<img$attributes>",
      );
    }

    // Include code entry form.
    $form = $this->getForm($form, $form_state);
    $form['actions']['login']['#value'] = t('Verify and save');

    // Alter code description.
    $form['code']['#description'] = t('A verification code will be generated after you scan the above QR code or manually enter the setup code. The verification code is six digits long.');

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function validateSetupForm(array $form, array &$form_state) {
    if (!$this->validate($form_state['values']['code'])) {
      $this->errorMessages['code'] = t('Invalid application code. Please try again.');

      return FALSE;
    }
    else {
      return TRUE;
    }
  }

  /**
   * {@inheritdoc}
   */
  protected function validate($code) {
    return $this->ga->verifyCode($this->seed, $code, $this->timeSkew);
  }

  /**
   * {@inheritdoc}
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
   *
   * @return string
   *   URL.
   */
  protected function getQrCodeUrl($seed) {
    // Note, this URL is over https but does leak the seed and account
    // email address to Google. See README.txt for local QR code generation
    // using qrcode.js.
    return $this->ga->getQRCodeGoogleUrl($this->accountName(), $seed);
  }

  /**
   * Create OTP seed for account.
   *
   * @return string
   *   Seed.
   */
  protected function createSeed() {
    return $this->ga->createSecret();
  }

  /**
   * Save seed for account.
   *
   * @param string $seed
   *   Seed.
   */
  protected function storeSeed($seed) {
    // Encrypt seed for storage.
    $encrypted = $this->encrypt($seed);
    // Data is binary so store base64 encoded.
    $record = array(
      'uid' => $this->context['uid'],
      'seed' => base64_encode($encrypted),
      'created' => time(),
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
   * @return string
   *   URL encoded string.
   */
  protected function accountName() {
    $account = user_load($this->context['uid']);

    return urlencode($this->namePrefix . '-' . $account->name);
  }

}
