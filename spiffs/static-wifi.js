/* Wi-Fi provisioning page. Requires jQuery and Bootstrap CSS. */
(function ($) {
    'use strict';

    $(function () {
        const $form = $('#wifi-form');
        if (!$form.length) return;

        const $message = $('#wifi-save-msg');
        const $submit = $('#wifi-submit');
        const $fields = $('#wifi-ssid, #wifi-password');
        const buttonText = $submit.text();
        const config = window.PressureSensorConfig || {};
        let busy = false;

        $message.attr({ role: 'status', 'aria-live': 'polite', 'aria-atomic': 'true' });

        function showMessage(type, text) {
            // Use text(), not html(): API messages must never become markup.
            $message.empty().append(
                $('<div>').addClass('alert alert-' + type).attr('role', 'alert').text(text)
            );
        }

        function setBusy(value, label) {
            busy = value;
            $form.attr('aria-busy', String(value));
            $fields.prop('disabled', value);
            $submit.prop('disabled', value).text(value ? label : buttonText);
        }

        function apiMessage(data, fallback) {
            return data && typeof data.message === 'string' && data.message.length
                ? data.message : fallback;
        }

        function errorMessage(xhr, textStatus, fallback) {
            if (xhr.responseJSON) return apiMessage(xhr.responseJSON, fallback);
            if (textStatus === 'timeout') return 'The request timed out. Check your connection to the device and try again.';
            if (xhr.status === 0) return 'Cannot reach the device. Check your connection to its Wi-Fi network.';
            if (textStatus === 'parsererror') return 'The device returned an invalid JSON response. The operation could not be confirmed.';
            return fallback + ' (HTTP ' + xhr.status + ')';
        }

        function postJson(url, data) {
            return $.ajax({
                url: url, // Relative URL preserves the current host AND port.
                method: 'POST',
                contentType: 'application/json; charset=utf-8',
                dataType: 'json',
                processData: false,
                timeout: 15000,
                data: JSON.stringify(data)
            });
        }

        function restartDevice() {
            setBusy(true, 'Restarting...');
            showMessage('info', 'Sending restart request...');

            postJson('/api/control', {
                device_id: config.deviceId,
                device_serial: config.deviceSerial,
                params: { mode: 1 },
                action: 1
            }).done(function (response) {
                if (response && response.status === 0) {
                    showMessage('success', 'Restart requested. Reconnect to your normal Wi-Fi network after the device restarts.');
                    // Keep the stale setup form disabled during restart.
                } else {
                    showMessage('danger', apiMessage(response, 'The device did not confirm the restart.'));
                    setBusy(false);
                }
            }).fail(function (xhr, textStatus) {
                // mode 1 calls esp_restart() immediately, often before an HTTP
                // response can be sent. A dropped connection is inconclusive.
                if (xhr.status === 0 || textStatus === 'timeout' || textStatus === 'parsererror') {
                    showMessage('warning',
                        'No restart confirmation was received. The device may have restarted before replying. ' +
                        'Wait a few seconds and reconnect to your normal Wi-Fi. If the device has not restarted, reload this page and try again.');
                } else {
                    showMessage('danger', errorMessage(xhr, textStatus, 'Restart request failed.'));
                    setBusy(false);
                }
            });
        }

        $form.on('submit', function (event) {
            event.preventDefault();
            if (busy || !this.reportValidity()) return;

            if (typeof config.deviceId !== 'string' || !config.deviceId ||
                typeof config.deviceSerial !== 'string' || !config.deviceSerial ||
                config.deviceId.indexOf('{VAL_') !== -1 || config.deviceSerial.indexOf('{VAL_') !== -1) {
                showMessage('danger', 'Device identity is missing. Reload the page from the device.');
                return;
            }

            // Preserve spaces: they may be part of the SSID or password.
            const ssid = $('#wifi-ssid').val();
            const password = $('#wifi-password').val();
            if (new TextEncoder().encode(ssid).length > 32) {
                showMessage('danger', 'The network name (SSID) must not exceed 32 bytes.');
                return;
            }

            setBusy(true, 'Saving...');
            showMessage('info', 'Saving Wi-Fi credentials...');

            postJson('/api/wifi/provision', {
                device_id: config.deviceId,
                device_serial: config.deviceSerial,
                ssid: ssid,
                password: password
            }).done(function (response) {
                setBusy(false);
                if (!response || response.status !== 0) {
                    showMessage('danger', apiMessage(response, 'The device did not confirm that Wi-Fi credentials were saved.'));
                    return;
                }

                showMessage('success', 'Wi-Fi credentials saved. Restart the device to use them. The connection has not been tested.');
                if (window.confirm('Wi-Fi credentials were saved successfully. Restart the device now to use them?')) {
                    restartDevice();
                }
            }).fail(function (xhr, textStatus) {
                setBusy(false);
                showMessage('danger', errorMessage(xhr, textStatus, 'Failed to save Wi-Fi credentials.'));
            });
        });
    });
})(jQuery);

$('#wifi-password-toggle').on('click', function () {
    const $password = $('#wifi-password');
    const showPassword = $password.attr('type') === 'password';

    $password.attr('type', showPassword ? 'text' : 'password');

    const label = showPassword ? 'Hide password' : 'Show password';

    $(this).attr({
        'aria-label': label,
        'title': label
    });

    $('#wifi-password-slash').toggleClass('d-none', !showPassword);
});
