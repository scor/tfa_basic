/**
 * @file
 * Documentation missing.
 */

(function ($) {
  'use strict';

  Drupal.behaviors.tfaBasicAdminSettings = function (context, settings) {
    var $form = $('form.tfa-admin-settings-form');

    $('input[name="tfa_login[tfa_basic_trusted_device]"]:not(.tfa-basic-admin-settings-processed)', $form)
      .addClass('tfa-basic-admin-settings-processed')
      .change(Drupal.tfaBasic.adminSettingsTrustedDeviceEnabledOnChange)
      .trigger('change');
  };

  Drupal.tfaBasic = Drupal.tfaBasic || {};

  Drupal.tfaBasic.adminSettingsTrustedDeviceEnabledOnChange = function () {
    var $form = $(this).parents('form').get(0);

    if (this.checked) {
      $('input[name="tfa_basic_cookie_domain"]', $form).parent().show();
    }
    else {
      $('input[name="tfa_basic_cookie_domain"]', $form).parent().hide();
    }
  };

}(jQuery));
