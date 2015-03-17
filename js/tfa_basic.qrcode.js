/**
 * @file
 * Documentation missing.
 */

/* global QRCode */
(function ($) {
  'use strict';

  Drupal.behaviors.tfaBasicQrCode = function (context) {
    if (!Drupal.hasOwnProperty('settings') ||
      !Drupal.settings.hasOwnProperty('tfaBasic') ||
      !Drupal.settings.tfaBasic.hasOwnProperty('qrCodes')
    ) {
      return;
    }

    var qrCodes = Drupal.settings.tfaBasic.qrCodes;
    for (var qrCodeId in qrCodes) {
      if (qrCodes.hasOwnProperty(qrCodeId)) {
        var $qrCode = $('#' + qrCodeId);

        if (!$qrCode.hasClass('tfa-qrcode-processed')) {
          $qrCode.addClass('tfa-qrcode-processed');
          var options = $.extend({}, Drupal.tfaBasicQrCode.defaults, qrCodes[qrCodeId]);
          new QRCode($qrCode.get(0), options);
        }
      }
    }
  };

  Drupal.tfaBasicQrCode = Drupal.tfaBasicQrCode || {};

  Drupal.tfaBasicQrCode.defaults = {
    width: 256,
    height: 256,
    colorDark: '#000000',
    colorLight: '#ffffff'
  };

}(jQuery));
