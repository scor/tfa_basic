Basic plugins for TFA module

Intent is to provide basic functionality of TFA Drupal module and to be an
example of TFA plugin development.

Plugins:

 * TOTP
 A Time-based One Time Password plugin using PHP_Gansta\GoogleAuthenticator
 PHP library.

 * Trusted device
 A TFA login plugin allowing browsers to be marked "trusted" so that subsequent
 logins will not require TFA for a 30 day window.

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
