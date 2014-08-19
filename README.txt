Basic plugins for TFA module

Intent is to provide basic functionality of TFA Drupal module and to be an
example of TFA plugin development.

Plugins:

 * TOTP
 A Time-based One Time Password plugin using PHP_Gansta\GoogleAuthenticator
 PHP library.

 * Trusted Browsers
 A TFA login plugin allowing browsers to be marked "trusted" so that subsequent
 logins will not require TFA for a 30 day window.

 * Recovery Codes
 Pre-generated one-time-use codes.

Variables:

 * tfa_basic_secret_key
 Secret key to to use as encryption key for TOTP seed encryption. Should be set
 in settings.php and not in the Drupal database.

 * tfa_basic_time_skew
 Number of 30 second chunks to allow TOTP keys between.

 * tfa_basic_name_prefix
 Prefix for TOTP QR code names. Suffix if "-username".

 * tfa_basic_cookie_name
 Cookie name of TFA trusted device cookie. Default is "TD".

 * tfa_basic_cookie_domain
 Cookie domain for TFA trusted device cookie.

 * tfa_basic_trust_cookie_expiration
 How long before TFA cookies expire. Default is 30 days.

### Using qrcode.js library instead of Google images

By default the module uses Google's Chart API to create a QR code. That request
will leak the seed to google in the URL of an HTTP GET request which reduces
the security of the seed. The module also supports the qrcode.js project which
will create the QR code image without leaking information to third-party sites.

To enable qrcode.js you simply have to place the library in the
tfa_basic/includes directory. From the command line:

  cd sites/all/modules/contrib/tfa_basic/includes/
  git clone git@github.com:davidshimjs/qrcodejs.git

The qrcode.min.js file should be at tfa_basic/includes/qrcodejs/qrcode.min.js

No additional setup is necessary, if the file exists in the right location then
it will be used.

