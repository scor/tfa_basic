<?php

/**
 * @param $account
 * @return string
 */
function tfa_basic_overview($account) {

  $output['info'] = array(
    '#type' => 'markup',
    '#markup' => '<p>' . t('Two-factor authentication (TFA) provides additional security for your account. With TFA enabled, you sign on to the site with a uniquely generated code in addition to your username and password.') . '</p>',
  );

  $data = tfa_basic_get_tfa_data($account);
  $enabled = isset($data['status']) && $data['status'] ? TRUE : FALSE;

  if (!empty($data)) {
    if ($enabled) {
      $status_text = t('Status: <strong>TFA enabled</strong>, set !time', array('!time' => format_date($data['saved'])));
    }
    else {
      $status_text = t('Status: <strong>TFA disabled</strong>, set !time', array('!time' => format_date($data['saved'])));
    }
    $output['status'] = array(
      '#type' => 'markup',
      '#markup' => '<p>' . $status_text . '</p>',
    );
  }

  // Application setup.
  $output['app'] = array(
    '#type' => 'fieldset',
    '#title' => t('TFA application'),
    '#description' => t('Generate TFA codes for sign on from a mobile or desktop application.'),
  );
  $setup_text = !$enabled ? t('Set up application') : t('Reset application');
  $output['app']['link'] = array(
    '#type' => 'markup',
    '#markup' => l($setup_text, 'user/' . $account->uid . '/security/tfa/app-setup'),
  );

  // Trusted browsers.
  $login_plugins = variable_get('tfa_login_plugins', array());
  if ($enabled && !empty($login_plugins) && in_array('tfa_basic_trusted_device', $login_plugins)) {
    $trusted_device = new TfaTrustedDeviceSetup(array('uid' => $account->uid));
    $trusted_browsers = array();
    foreach ($trusted_device->getTrustedDevices() as $device) {
      $vars = array(
        '!set' => format_date($device['created']),
        '@browser' => $device['name'],
        '!time' => format_date($device['last_used']),
      );
      if (empty($device['last_used'])) {
        $message = t('@browser, set !set', $vars);
      }
      else {
        $message = t('@browser, set !set, last used !time', $vars);
      }
      $trusted_browsers[] = $message;
    }
    $time = variable_get('tfa_basic_trust_cookie_expiration', 3600 * 24 * 30) / (3600 * 24);
    $output['trust'] = array(
      '#type' => 'fieldset',
      '#title' => t('Trusted browsers'),
      '#description' => t('Browsers that will not require TFA during sign on for !time days from first set.', array('!time' => $time)),
    );
    if (!empty($trusted_browsers)) {
      $output['trust']['list'] = array(
        '#type' => 'markup',
        '#markup' => theme('item_list', array('items' => $trusted_browsers)),
      );
    }
    $output['trust']['link'] = array(
      '#type' => 'markup',
      '#markup' => l(t('Set trusted browsers'), 'user/' . $account->uid . '/security/tfa/trusted-browsers'),
    );
  }

  // Recovery codes.
  if ($enabled && in_array('tfa_basic_recovery_code', variable_get('tfa_fallback_plugins', array()))) {
    $recovery = new TfaBasicRecoveryCodeSetup(array('uid' => $account->uid));
    $output['recovery'] = array(
      '#type' => 'fieldset',
      '#title' => t('Recovery codes'),
      '#description' => t('Pre-generated, one-time-use codes that allow sign on instead of the TFA application code.'),
    );
    $recovery_codes = $recovery->getCodes();
    if (empty($recovery_codes)) {
      $codes_text = t('Get recovery codes');
    }
    else {
      $output['recovery']['list'] = array(
        '#type' => 'markup',
        '#markup' => '<p>' . l(t('View unused recovery codes'), 'user/' . $account->uid . '/security/tfa/recovery-codes-list') . '</p>',
      );
      $codes_text = t('Get new recovery codes');
    }
    $output['recovery']['link'] = array(
      '#type' => 'markup',
      '#markup' => l($codes_text, 'user/' . $account->uid . '/security/tfa/recovery-codes'),
    );
  }

  if ($enabled) {
    $output['disable'] = array(
      '#type' => 'markup',
      '#markup' => '<p>' . l(t('Disable TFA'), 'user/' . $account->uid . '/security/tfa/disable') . '</p>',
    );
  }

  return drupal_render($output);
}

function tfa_basic_disable_form($form, &$form_state, $account) {
  global $user;

  $form_state['storage']['account'] = $account;

  if ($account->uid != $user->uid && user_access('administer users')) {
    $preamble_desc = t('Are you sure you want to disable TFA on account %name?', array('%name' => $account->name));
    $notice_desc = t('TFA settings and data will be lost. %name can re-enable TFA again from their profile.', array('%name' => $account->name));
    if (user_access('require tfa', $account)) {
      drupal_set_message(t("This account is required to have TFA enabled per the 'require TFA' permission on one of their roles. Disabling TFA will remove their ability to log back into the site. If you continue, consider also removing the role so they can authenticate and setup TFA again."), 'warning');
    }
  }
  else {
    $preamble_desc = t('Are you sure you want to disable your two-factor authentication setup?');
    $notice_desc = t("Your settings and data will be lost. You can re-enable two-factor authentication again from your profile.");
    if (user_access('require tfa', $account)) {
      drupal_set_message(t('Your account must have at least one two-factor authentication method enabled. Continuing will disable your ability to log back into this site.'), 'warning');
      $notice_desc = t('Your settings and data will be lost and you will be unable to log back into the site. To regain access contact a site administrator.');
    }
  }
  $form['preamble'] = array(
    '#prefix' => '<p class="preamble">',
    '#suffix' => '</p>',
    '#markup' => $preamble_desc,
  );
  $form['notice'] = array(
    '#prefix' => '<p class="preamble">',
    '#suffix' => '</p>',
    '#markup' => $notice_desc,
  );

  $form['account']['current_pass'] = array(
    '#type' => 'password',
    '#title' => t('Confirm your current password'),
    '#description_display' => 'before',
    '#size' => 25,
    '#weight' => -5,
    '#attributes' => array('autocomplete' => 'off'),
    '#required' => TRUE,
  );
  $form['account']['mail'] = array(
    '#type' => 'value',
    '#value' => $account->mail,
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Disable'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'submit',
    '#value' => t('Cancel'),
    '#limit_validation_errors' => array(),
    '#submit' => array('tfa_basic_disable_form_submit'),
  );

  return $form;
}

/**
 * Disable form validate.
 */
function tfa_basic_disable_form_validate($form, &$form_state) {
  $account = $form_state['storage']['account'];
  // Check password. (from user.module user_validate_current_pass()).
  require_once DRUPAL_ROOT . '/' . variable_get('password_inc', 'includes/password.inc');
  $current_pass = user_check_password($form_state['values']['current_pass'], $account);
  if (!$current_pass) {
    form_set_error('current_pass', t("Incorrect password."));
  }
}

/**
 * Disable form submit.
 */
function tfa_basic_disable_form_submit($form, &$form_state) {
  $account = $form_state['storage']['account'];
  if ($form_state['values']['op'] === $form_state['values']['cancel']) {
    drupal_set_message(t('TFA disable canceled.'));
    $form_state['redirect'] = 'user/' . $account->uid . '/security/tfa';
    return;
  }
  $params = array('account' => $account);
  tfa_basic_setup_save_data($account, array('status' => FALSE));

  watchdog('tfa', 'TFA disabled for user @name UID !uid', array(
    '@name' => $account->name,
    '!uid' => $account->uid,
  ), WATCHDOG_NOTICE);

  // E-mail account to inform user that it has been disabled.
  drupal_mail('tfa_basic', 'disable', $account->mail, user_preferred_language($account), $params);

  drupal_set_message(t('TFA has been disabled.'));
  $form_state['redirect'] = 'user/' . $account->uid . '/security/tfa';
}

/**
 * TFA setup form router.
 */
function tfa_basic_setup_form($form, &$form_state, $account, $method = 'app_setup') {
  global $user;

  $form['account'] = array(
    '#type' => 'value',
    '#value' => $account,
  );
  $tfa_data = tfa_basic_get_tfa_data($account);

  // Do not continue if this isn't app setup and the app isn't already set.
  if (empty($tfa_data['status']) && $method != 'app_setup') {
    drupal_set_message(t('Set up TFA application before continuing with additional configurations.'), 'warning');
    drupal_goto('user/' . $account->uid . '/security/tfa');
  }

  if (empty($form_state['storage'])) {

    // Allow administrators to change TFA settings for another account.
    if ($account->uid != $user->uid && user_access('administer users')) {
      $current_pass_description = t('Enter your current password to alter TFA settings for account %name.', array('%name' => $account->name));
    }
    else {
      $current_pass_description = t('Enter your current password to continue with two-factor authentication setup.');
    }
    $form['current_pass'] = array(
      '#type' => 'password',
      '#title' => t('Current password'),
      '#size' => 25,
      '#required' => TRUE,
      '#description' => $current_pass_description,
      '#attributes' => array('autocomplete' => 'off'),
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Confirm'),
    );
    $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Cancel'),
      '#limit_validation_errors' => array(),
      '#submit' => array('tfa_basic_setup_form_submit'),
    );
  }
  else {
    // Support multi-step TFA plugins triggered in tfa_basic_setup_form_submit.
    if (isset($form_state['storage']['method'])) {
      $method = $form_state['storage']['method'];
    }
    switch ($method) {
      case 'app_setup':
        drupal_set_title(t('TFA setup - Application'));
        $form_state['storage']['methods'][] = $method;
        $tfa_setup = new TfaSetup(array('setup' => 'TfaTotpSetup'), array('uid' => $account->uid));

        if (!empty($tfa_data)) {
          $form['disclaimer'] = array(
            '#type' => 'markup',
            '#markup' => '<p>' . t('Note: You should delete the old account in your mobile or desktop app before adding this new one.') . '</p>',
          );
        }
        $form = $tfa_setup->getForm($form, $form_state);
        $form_state['storage'][$method] = $tfa_setup;
        break;

      case 'trust_device':
        drupal_set_title(t('TFA setup - Trusted browsers'));
        $form_state['storage']['methods'][] = $method;
        $tfa_setup = new TfaSetup(array('setup' => 'TfaTrustedDeviceSetup'), array('uid' => $account->uid));
        $form = $tfa_setup->getForm($form, $form_state);
        $form_state['storage'][$method] = $tfa_setup;
        break;

      case 'recovery_codes':
        drupal_set_title(t('TFA setup - Recovery codes'));
        $form_state['storage']['methods'][] = $method;
        $tfa_setup = new TfaSetup(array('setup' => 'TfaBasicRecoveryCodeSetup'), array('uid' => $account->uid));
        $form = $tfa_setup->getForm($form, $form_state);
        $form_state['storage'][$method] = $tfa_setup;
        break;

      default:
        break;
    }
    // Provide skip button for optional TFA plugins.
    if (count($form_state['storage']['methods']) > 1) {
      $form['actions']['skip'] = array(
        '#type' => 'submit',
        '#value' => t('Skip'),
        '#limit_validation_errors' => array(),
        '#submit' => array('tfa_basic_setup_form_submit'),
      );
    }
    $form_state['storage']['method'] = $method;

    $form['actions']['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Cancel'),
      '#limit_validation_errors' => array(),
      '#submit' => array('tfa_basic_setup_form_submit'),
    );
  }

  return $form;

}

/**
 * Setup form validate.
 */
function tfa_basic_setup_form_validate($form, &$form_state) {
  global $user;
  $account = $form['account']['#value'];
  if (isset($form_state['values']['current_pass'])) {
    // Allow administrators to change TFA settings for another account.
    if ($account->uid != $user->uid && user_access('administer users')) {
      $account = $user;
    }
    // Check password. (from user.module user_validate_current_pass()).
    require_once DRUPAL_ROOT . '/' . variable_get('password_inc', 'includes/password.inc');
    $current_pass = user_check_password($form_state['values']['current_pass'], $account);
    if (!$current_pass) {
      form_set_error('current_pass', t("Incorrect password."));
    }
    return;
  }
  elseif (isset($form_state['values']['cancel']) && $form_state['values']['op'] === $form_state['values']['cancel']) {
    return;
  }
  elseif (!empty($form_state['storage']['method'])) {
    $method = $form_state['storage']['method'];
    $tfa_setup = $form_state['storage'][$method];
    if (!$tfa_setup->validateForm($form, $form_state)) {
      foreach ($tfa_setup->getErrorMessages() as $element => $message) {
        form_set_error($element, $message);
      }
    }
  }
}

/**
 * Setup form submit.
 */
function tfa_basic_setup_form_submit($form, &$form_state) {
  $account = $form['account']['#value'];

  // Cancel button.
  if (isset($form_state['values']['cancel']) && $form_state['values']['op'] === $form_state['values']['cancel']) {
    drupal_set_message('TFA setup canceled.');
    $form_state['redirect'] = 'user/' . $account->uid . '/security/tfa';
    return;
  }
  // Password validation.
  if (isset($form_state['values']['current_pass'])) {
    $form_state['storage']['pass_confirmed'] = TRUE;
    $form_state['rebuild'] = TRUE;
    return;
  }
  // Submitting a plugin form.
  elseif (!empty($form_state['storage']['method'])) {
    $method = $form_state['storage']['method'];
    // Trigger multi-step for optional config after app_setup.
    if ($method == 'app_setup') {
      $form_state['rebuild'] = TRUE;
      $form_state['storage']['method'] = 'trust_device';
      $form_state['storage']['values'][$method] = $form_state['values'];
      drupal_set_message(t('Mark this browser as trusted or skip to continue and finish TFA setup.'));
      return;
    }
    // Support skipping optional steps.
    elseif (isset($form_state['values']['skip']) && $form_state['values']['op'] === $form_state['values']['skip']) {
      $skipped_method = array_pop($form_state['storage']['methods']);
      unset($form_state['storage'][$skipped_method]);
    }
    // Trigger multi-step for optional recovery codes after trusted browser.
    if ($method == 'trust_device' && in_array('app_setup', $form_state['storage']['methods'])) {
      $form_state['rebuild'] = TRUE;
      $form_state['storage']['method'] = 'recovery_codes';
      $form_state['storage']['values'][$method] = $form_state['values'];
      drupal_set_message(t('Save recovery codes or skip to complete TFA setup.'));
      return;
    }
    // Form values are modified for each submitted plugin but need to be
    // restored for possible form alters.
    $values = $form_state['values'];

    $tfa_data = array(
      'method' => 'app',
    );
    // Run submit handler for each finished plugin.
    foreach ($form_state['storage']['methods'] as $method) {
      $setup_class = $form_state['storage'][$method];

      if (isset($form_state['storage']['values'][$method])) {
        // Pass plugin-specific form values to plugin submit handlers.
        $form_state['values'] = $form_state['storage']['values'][$method];
      }
      if (!$setup_class->submitForm($form, $form_state)) {
        drupal_set_message(t('There was an error during TFA setup. Your settings have not been saved.'), 'error');
        $form_state['redirect'] = 'user/' . $account->uid . '/security/tfa';
        return;
      }
    }

    // Save TFA data.
    tfa_basic_setup_save_data($account, $tfa_data);
    // Setup complete, return to overview page.
    drupal_set_message(t('TFA setup complete.'));
    $form_state['redirect'] = 'user/' . $account->uid . '/security/tfa';

    // Log and notify if app was set up.
    if (in_array('app_setup', $form_state['storage']['methods'])) {
      $params = array('account' => $account);
      drupal_mail('tfa_basic', 'tfa_basic_tfa_enabled', $account->mail, user_preferred_language($account), $params);
      watchdog('tfa', 'TFA enabled for user @name UID !uid', array(
        '@name' => $account->name,
        '!uid' => $account->uid,
      ), WATCHDOG_INFO);
    }

    // Restore form values.
    $form_state['values'] = $values;
  }
}

/**
 * Output unused recovery codes.
 *
 * @param stdClass $account
 * @return string
 */
function tfa_basic_recovery_codes_list($account) {
  $recovery = new TfaBasicRecoveryCodeSetup(array('uid' => $account->uid));
  $codes = $recovery->getCodes();

  $output = theme('item_list', array('items' => $codes));
  $output .= l(t('Return to account TFA overview'), 'user/' . $account->uid . '/security/tfa');
  return $output;
}