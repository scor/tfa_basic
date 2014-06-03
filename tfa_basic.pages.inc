<?php

/**
 * @param $account
 * @return string
 */
function tfa_basic_overview($account) {

  $data = tfa_basic_get_tfa_data($account);

  $edit_url = url('user/' . $account->uid . '/tfa/setup', array('absolute' => TRUE));
  if (!empty($data)) {
    $status = $data['status'] ? t('Enabled') : t('Disabled');
    $output['status'] = array(
      '#type' => 'markup',
      '#markup' => '<p>'. t('Two-factor authentication status: <strong>!status</strong>', array('!status' => $status)) . '</p>',
    );
    if ($data['status']) {
      $output['method'] = array(
        '#type' => 'markup',
        '#markup' => '<p>'. t('Method: <strong>Application</strong>') . '</p>',
      );
      $devices = tfa_basic_get_trusted_devices($account);
      if (!empty($devices)) {
        $items = array();
        foreach ($devices as $device) {
          $message = t('@browser last used !time', array('@browser' => $device['name'], '!time' => !empty($device['last_used']) ? format_date($device['last_used']) : t('never')));
          $items[] = $message;
        }
        $output['trusted'] = array(
          '#type' => 'markup',
          '#markup' => theme('item_list', array('items' => $items, 'title' => t('Trusted browsers'))),
        );
      }

    }
    $output['time'] = array(
      '#type' => 'markup',
      '#markup' => '<p>'. t('Data last saved !time', array('!time' => format_date($data['saved']))) . '</p>'
    );
    $output['setup'] = array(
      '#type' => 'markup',
      '#markup' => '<p>'. l('Alter two-factor authentication for your account', $edit_url) . '</p>',
    );
  }
  else {
    $output['setup'] = array(
      '#type' => 'markup',
      '#markup' => '<p>'. l('Set up two-factor authentication for your account', $edit_url) . '</p>',
    );
  }

  return drupal_render($output);
}

function tfa_basic_disable_form($form, &$form_state, $account) {

  $form_state['storage']['account'] = $account;

  $form['preamble'] = array(
    '#prefix' => '<p class="preamble">',
    '#suffix' => '</p>',
    '#markup' => t('Are you sure you want to disable your two-factor authentication setup?'),
  );
  $form['notice'] = array(
    '#prefix' => '<p class="preamble">',
    '#suffix' => '</p>',
    '#markup' => t("Your setttings and data will be lost. You can re-enable two-factor authentication again from your profile."),
  );
  if (user_access('require tfa', $account)) {
    drupal_set_message(t('Your account must have at least one two-factor authentication method enabled. Continuing will disable your ability to log back into this site.'), 'warning');
    $form['notice']['#markup'] = t('Your setttings and data will be lost and you will be unable to log back into the site. To regain access contact a site administrator.');
  }

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
    drupal_set_message('TFA setup canceled');
    $form_state['redirect'] = 'user/' . $account->uid . '/tfa';
    return;
  }
  $params = array('account' => $account);
  tfa_basic_setup_save_data($account, array('status' => FALSE));

  watchdog('tfa', 'TFA disabled for user @name UID !uid', array(
    '@name' => $account->name,
    '!uid' => $account->uid,
  ), WATCHDOG_NOTICE);

  // E-mail account to inform them that it has been disabled.
  drupal_mail('tfa_basic', 'disable', $account->mail, user_preferred_language($account), $params);

  drupal_set_message(t('Two-factor authentication is now disabled.'));
  $form_state['redirect'] = 'user/' . $account->uid . '/tfa';
}

/**
 * TFA setup form router.
 */
function tfa_basic_setup_form($form, &$form_state, $account) {
  $form['account'] = array(
    '#type' => 'value',
    '#value' => $account,
  );
  if (empty($form_state['storage'])) {

    $data = tfa_basic_get_tfa_data($account);

    if (!empty($data) && $data['status']) {
      $form['disable'] = array(
        '#prefix' => '<p>',
        '#suffix' => '</p>',
        '#markup' => l(t('Disable two-factor verification'), 'user/' . $account->uid . '/tfa/disable'),
      );
    }
    // Setup selection.
    $form['app'] = array(
      '#prefix' => '<div>',
      '#suffix' => '</div>',
      '#type' => 'submit',
      '#value' => 'Setup application',
    );
    $form['trust'] = array(
      '#prefix' => '<div>',
      '#suffix' => '</div>',
      '#type' => 'submit',
      '#value' => 'Mark browser trusted',
    );
  }
  else {
    $method = $form_state['storage']['method'];
    switch ($method) {
      case 'app':
        drupal_set_title(t('TFA setup - Application'));
        break;
    }
    if ($method == 'app') {
      $existing = tfa_basic_get_tfa_data($account);
      if (!empty($existing)) {
        if ($method == 'app') {
          $output['disclaimer'] = array(
            '#type' => 'markup',
            '#markup' => '<p>' . t('Note: You should delete the old corresponding account in your smartphone or tablet app before adding this new one.') . '</p>',
          );
        }
      }
    }

    $tfaSetup = $form_state['storage']['tfa_setup'];
    $form = $tfaSetup->getForm($form, $form_state);

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
  if (empty($form_state['storage']['tfa_setup'])) {
    return;
  }
  elseif ($form_state['values']['op'] === $form_state['values']['cancel']) {
    return;
  }
  $account = $form['account']['#value'];
  // Check password. (from user.module user_validate_current_pass()).
  require_once DRUPAL_ROOT . '/' . variable_get('password_inc', 'includes/password.inc');
  $current_pass = user_check_password($form_state['values']['current_pass'], $account);
  if (!$current_pass) {
    form_set_error('current_pass', t("Incorrect password."));
  }

  $tfaSetup = $form_state['storage']['tfa_setup'];
  if (!$tfaSetup->validateForm($form, $form_state)) {
    foreach ($tfaSetup->getErrorMessages() as $element => $message) {
      form_set_error($element, $message);
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
    drupal_set_message('TFA setup canceled');
    $form_state['redirect'] = 'user/' . $account->uid . '/tfa';
    return;
  }

  // A first step: create TfaSetup class for Application setup.
  if (isset($form_state['values']['app']) && $form_state['values']['op'] === $form_state['values']['app']) {
    $method = 'app';
    $form_state['storage']['method'] = $method;
    $tfaSetup = new TfaSetup(array('setup' => 'TfaTotpSetup'), array('uid' => $account->uid));
    $form_state['storage']['tfa_setup'] = $tfaSetup;
    $form_state['rebuild'] = TRUE;
  }
  // A first step: start trust device.
  elseif (isset($form_state['values']['trust']) && $form_state['values']['op'] === $form_state['values']['trust']) {
    $method = 'trust';
    $form_state['storage']['method'] = $method;
    $tfaSetup = new TfaSetup(array('setup' => 'TfaTrustedDeviceSetup'), array('uid' => $account->uid));
    $form_state['storage']['tfa_setup'] = $tfaSetup;
    $form_state['rebuild'] = TRUE;
  }
  // A final step: submit TfaSetup form.
  elseif (!empty($form_state['storage']['tfa_setup'])) {
    $tfa_data = array();
    $method = $form_state['storage']['method'];
    $tfaSetup = $form_state['storage']['tfa_setup'];
    // Fake trust_device for CciTfaTrustedDevice#submit
    if ($method == 'trust') {
      $form_state['values']['trust_device'] = TRUE;
    }
    if ($tfaSetup->submitForm($form, $form_state)) {
      if ($method == 'app') {
        $tfa_data['method'] = $method;
      }
      // Save data.
      tfa_basic_setup_save_data($account, $tfa_data);
      drupal_set_message('TFA setup complete');
      $form_state['redirect'] = 'user/' . $account->uid . '/tfa';
      // Log/notify appropriately.
      $params = array('account' => $account);
      drupal_mail('tfa_basic', 'tfa_basic_tfa_enabled', $account->mail, user_preferred_language($account), $params);
      watchdog('tfa', 'TFA enabled for user @name UID !uid', array(
        '@name' => $account->name,
        '!uid' => $account->uid,
      ), WATCHDOG_NOTICE);

    }
    else {
      // Setup isn't complete so rebuild.
      $form_state['rebuild'] = TRUE;
    }
  }
}
