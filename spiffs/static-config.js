// script-config.js

function showConfigMessage(msg, isError) {
  // You currently render: <h5>{VAL_MESSAGE}</h5>
  // If you keep it, add id="cfg_msg" to that element (recommended).
  // Fallback: try to find it anyway.
  const $msg = $("#cfg_msg");
  if ($msg.length) {
    $msg.text(msg || "");
    $msg.toggleClass("text-danger", !!isError);
    $msg.toggleClass("text-success", !isError && !!msg);
  } else {
    // no dedicated element; at least log
    if (isError) console.error(msg);
    else console.log(msg);
  }
}

// Config page logic (CA cert lazy-load)
// Expect HTML to set: window.PressureSensorConfig = { deviceId, deviceSerial }
$(function () {
  if (!window.PressureSensorConfig) {
    return; // not on config page (or config not injected)
  }

  const deviceId = window.PressureSensorConfig.deviceId;
  const deviceSerial = window.PressureSensorConfig.deviceSerial;

  const certLoaded = { mqtts: false, https: false };

  function getTextarea(caType) {
    return (caType === "mqtts") ? $("#ca_cert_mqtts_id") : $("#ca_cert_https_id");
  }

  function loadCaCert(caType, doneCb) {
    if (certLoaded[caType]) {
      if (doneCb) doneCb();
      return;
    }

    const $textarea = getTextarea(caType);

    $textarea
      .prop("disabled", true)
      .val("Loading " + caType.toUpperCase() + " CA certificate...");

    $.ajax({
      url: "/api/cert/get",
      method: "GET",
      dataType: "json",
      data: {
        device_id: deviceId,
        device_serial: deviceSerial,
        ca_type: caType
      },
      timeout: 10000
    })
      .done(function (resp) {
        if (resp && resp.status === 0 && typeof resp.cert === "string") {
          $textarea.val(resp.cert);
          certLoaded[caType] = true;
        } else {
          const msg = (resp && resp.msg) ? resp.msg : "unknown error";
          $textarea.val(
            "ERROR: Failed to load " + caType.toUpperCase() + " CA certificate\n\n" + msg
          );
        }
      })
      .fail(function (xhr, status) {
        $textarea.val(
          "ERROR: Failed to load " + caType.toUpperCase() + " CA certificate\n\n" +
          "HTTP error: " + status
        );
      })
      .always(function () {
        $textarea.prop("disabled", false);
        if (doneCb) doneCb();
      });
  }

  // Sequential load on page load: MQTTS -> HTTPS
  loadCaCert("mqtts", function () {
    loadCaCert("https");
  });
});

// Load and populate config settings form
(function () {

  function getField(data, key) {
    // data[key] is expected as { type, max_size, value, size }
    if (!data || !data[key]) return null;
    return data[key];
  }

  function fieldValue(field) {
    if (!field) return null;
    return field.value;
  }

  function setInputByName(name, field) {
    const $el = $(`[name="${name}"]`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));

    // Optional: apply maxlength if present and input is text-like
    if (field && field.max_size && $el.attr("type") !== "number") {
      $el.attr("maxlength", String(field.max_size));
    }
  }

  function setInputById(id, field) {
    const $el = $(`#${id}`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));

    // Optional: apply maxlength if present and input is text-like
    if (field && field.max_size && $el.attr("type") !== "number") {
      $el.attr("maxlength", String(field.max_size));
    }
  }

  function setNumberByName(name, field) {
    const $el = $(`[name="${name}"]`);
    if (!$el.length) return;

    const v = fieldValue(field);
    $el.val(v == null ? "" : String(v));
  }

  function setSelectById(id, field) {
    const $el = $(`#${id}`);
    if (!$el.length) return;

    const v = fieldValue(field);
    if (v == null) return;

    $el.val(String(v));
  }

  function populateForm(data) {
    // MQTT
    setSelectById("mqtt_connect", getField(data, "mqtt_connect"));
    setInputByName("mqtt_server",   getField(data, "mqtt_server"));
    setNumberByName("mqtt_port",    getField(data, "mqtt_port"));
    setInputByName("mqtt_protocol", getField(data, "mqtt_protocol"));
    setInputByName("mqtt_user",     getField(data, "mqtt_user"));
    setInputByName("mqtt_password", getField(data, "mqtt_password"));
    setInputByName("mqtt_prefix",   getField(data, "mqtt_prefix"));

    // Sensor specific data
    setNumberByName("sensor_intervl", getField(data, "sensor_intervl"));
    // offset shall be rouded to 3 decimal places for better UX (e.g., "1.234")
    const offsetField = getField(data, "sensor_offset");
    if (offsetField && offsetField.value != null) {
      offsetField.value = Math.round(offsetField.value * 1000) / 1000;
    }
    setNumberByName("sensor_offset", offsetField);
    setNumberByName("sensor_multipl", getField(data, "sensor_multipl"));
    setNumberByName("sensor_samples", getField(data, "sensor_samples"));
    setNumberByName("sensor_smp_int", getField(data, "sensor_smp_int"));
    setNumberByName("sensor_deviate", getField(data, "sensor_deviate"));

    // Home Assistant
    setInputByName("ha_prefix",       getField(data, "ha_prefix"));
    setNumberByName("ha_upd_intervl", getField(data, "ha_upd_intervl"));

    // OTA
    setInputByName("ota_update_url",  getField(data, "ota_update_url"));
    setSelectById("ota_upd_rescfg",   getField(data, "ota_upd_rescfg"));

    // Net logging
    setSelectById("net_log_type",    getField(data, "net_log_type"));
    setInputByName("net_log_host",   getField(data, "net_log_host"));
    setNumberByName("net_log_port",  getField(data, "net_log_port"));
    setSelectById("net_log_stdout",  getField(data, "net_log_stdout"));

    // memguard
    setSelectById("memgrd_mode",     getField(data, "memgrd_mode"));
    setNumberByName("memgrd_trshld", getField(data, "memgrd_trshld"));
  }

  function loadAllSettings() {
    if (!window.PressureSensorConfig) return;

    const deviceId = window.PressureSensorConfig.deviceId;
    const deviceSerial = window.PressureSensorConfig.deviceSerial;

    showConfigMessage("Loading settings...", false);

    $.ajax({
      url: "/api/setting/get/all",
      method: "GET",
      dataType: "json",
      data: {
        device_id: deviceId,
        device_serial: deviceSerial
      },
      timeout: 10000
    })
    .done(function (resp) {
      if (!resp || typeof resp !== "object") {
        showConfigMessage("ERROR: invalid API response", true);
        return;
      }

      // Your schema: { data: { key: {.., value: ..}}, total: N }
      if (!resp.data || typeof resp.data !== "object") {
        showConfigMessage("ERROR: response.data missing", true);
        return;
      }

      populateForm(resp.data);
      showConfigMessage("", false);
    })
    .fail(function (xhr, status) {
      showConfigMessage("ERROR: failed to load settings (" + status + ")", true);
    });
  }

  $(function () {
    loadAllSettings();
  });

})();

(function () {

  function showSaveMsg(text, isError) {
    const $m = $("#cfg_save_msg");
    if (!$m.length) return;

    $m.removeClass("alert alert-success alert-danger");
    if (!text) {
      $m.text("");
      return;
    }

    $m.addClass("alert " + (isError ? "alert-danger" : "alert-success"));
    $m.text(text);
  }

  function clearFieldErrors($form) {
    $form.find(".cfg-field-error").remove();
    $form.find(".is-invalid").removeClass("is-invalid");
  }

  function showFieldError($form, fieldName, message) {
    const $field = $form.find(`[name="${fieldName}"]`);
    if (!$field.length) return;

    $field.addClass("is-invalid");

    // Put error message right after field
    const $err = $(`<div class="cfg-field-error invalid-feedback" style="display:block;"></div>`);
    $err.text(message || "Invalid value");
    $field.after($err);
  }

  function collectFormData($form) {
    // Collect only your inputs/selects (excluding submit/reset)
    const data = {};
    $form.find("input, select, textarea").each(function () {
      const $el = $(this);
      const name = $el.attr("name");
      if (!name) return;

      const type = ($el.attr("type") || "").toLowerCase();
      if (type === "submit" || type === "reset" || type === "button") return;

      if (type === "checkbox") {
        // your backend probably expects 0/1 or bool; choose 0/1 for embedded friendliness
        data[name] = $el.is(":checked") ? 1 : 0;
      } else {
        if (type === "number") {
          const v = $el.val();
          // Convert to number or null if empty/invalid
          data[name] = (v === "") ? null : Number(v);
        } else {
          if (type === "select" || $el.is("select")) {
            // try to convert to number if possible
            const v = $el.val();
            const n = Number(v);
            data[name] = (v === "" || isNaN(n)) ? v : n;
          } else {
            // text, textarea, password, etc.
            data[name] = $el.val();
          }
        }
      }
    });

    return data;
  }

  function applyPerFieldErrors($form, resp) {
    // Supports resp.data[field].status/msg OR resp.fields[field].status/msg
    const map = (resp && resp.data && typeof resp.data === "object") ? resp.data :
                (resp && resp.fields && typeof resp.fields === "object") ? resp.fields :
                null;

    if (!map) return false;

    let anyFieldErrors = false;

    Object.keys(map).forEach(function (k) {
      const entry = map[k];
      if (!entry || typeof entry !== "object") return;

      const st = (typeof entry.status === "number") ? entry.status : null;
      if (st !== null && st !== 0) {
        anyFieldErrors = true;
        showFieldError($form, k, entry.msg || ("Error code: " + st));
      }
    });

    return anyFieldErrors;
  }

  function bindAjaxSave() {
    const $form = $('form[action="/submit"]');
    if (!$form.length) return;

    $form.on("submit", function (e) {
      e.preventDefault();

      clearFieldErrors($form);
      showSaveMsg("Saving settings...", false);

      const cfg = window.PressureSensorConfig || {};
      const deviceId = cfg.deviceId || "";
      const deviceSerial = cfg.deviceSerial || "";

      const formData = collectFormData($form);

      // If your API expects nested: { data: { device_id, device_serial, ...fields } }
      // this matches your other API styles.
      // Build payload EXACTLY as your API expects
      const payload = {
        device_id: deviceId,
        device_serial: deviceSerial,
        action: 0,          // 0 = update (per your API)
        data: formData      // ONLY settings here
      };

      $.ajax({
        url: "/api/setting/update",
        type: "POST",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(payload),
        timeout: 15000
      })
      .done(function (resp) {
        const anyFieldErrors = applyPerFieldErrors($form, resp);

        // Determine global status
        const st = (resp && typeof resp.status === "number") ? resp.status : (anyFieldErrors ? 1 : 0);
        const msg = (resp && resp.msg) ? resp.msg : (st === 0 ? "Saved successfully" : "Failed to save");

        if (st === 0 && !anyFieldErrors) {
          showSaveMsg(msg, false);
        } else {
          showSaveMsg("ERROR: " + msg, true);
        }
      })
      .fail(function (xhr, status) {
        showSaveMsg("ERROR: Save failed (" + status + ")", true);
      });
    });
  }

  $(function () {
    bindAjaxSave();
  });

})();

(function () {
  // --- OTA "new version available" check ---
  function getUpdateButtonHtml() {
    return `
      <div class="mt-2">
        <button type="button"
                id="otaUpdateBtn"
                class="btn btn-primary btn-sm">
          Update Device
        </button>
      </div>
    `;
  }

  function normalizeVersionParts(ver) {
    // "1.0.6" -> [1,0,6]
    // "v1.2.3" -> [1,2,3]
    // "1.2.3-rc1" -> [1,2,3] (best-effort)
    if (ver == null) return [];
    const s = String(ver).trim().replace(/^v/i, "");
    const main = s.split("-")[0]; // drop pre-release suffix if any
    return main
      .split(".")
      .map(p => {
        const n = parseInt(p, 10);
        return Number.isFinite(n) ? n : 0;
      });
  }

  function compareSemver(a, b) {
    // returns: -1 if a<b, 0 if a==b, 1 if a>b
    const ap = normalizeVersionParts(a);
    const bp = normalizeVersionParts(b);
    const len = Math.max(ap.length, bp.length);

    for (let i = 0; i < len; i++) {
      const av = (i < ap.length) ? ap[i] : 0;
      const bv = (i < bp.length) ? bp[i] : 0;
      if (av < bv) return -1;
      if (av > bv) return 1;
    }
    return 0;
  }

  function normalizeBuildNum(bn) {
    // Expect "YYYYMMDDHHMMSS" as string.
    // Return string of digits only to compare lexicographically safely.
    if (bn == null) return "";
    return String(bn).trim().replace(/[^\d]/g, "");
  }

  function deriveBuildInfoUrl(otaBinUrl) {
    // otaBinUrl points to .../ESPRelayBoard.bin (maybe with query params)
    // build_info.json is in the same directory.
    try {
      const u = new URL(otaBinUrl, window.location.href);
      // remove filename
      const path = u.pathname || "";
      const dir = path.replace(/\/[^\/]*$/, "/");
      u.pathname = dir + "build_info.json";
      u.search = ""; // drop query
      u.hash = "";
      return u.toString();
    } catch (e) {
      return null;
    }
  }

  function ensureOtaMsgContainer() {
    // Insert a Bootstrap alert right under the OTA URL input if not present
    let $box = $("#ota_new_version_msg");
    if ($box.length) return $box;

    const $ota = $("#ota_update_url");
    if (!$ota.length) return $(); // empty

    $box = $('<div id="ota_new_version_msg" class="mt-2"></div>');
    $ota.closest("td").append($box);
    return $box;
  }

  function renderOtaMsg(text, kind) {
    // kind: "info" | "success" | "warning" | "danger" | null
    const $box = ensureOtaMsgContainer();
    if (!$box.length) return;

    if (!text) {
      $box.empty();
      return;
    }

    const isDismissible = (kind === "success" || kind === "warning" || kind === "danger");

    // Remember dismissal ONLY for warning/danger (real notifications)
    const msgKey = isDismissible
      ? ("ota_notice_dismissed:" + kind + ":" + String(text))
      : null;

    if (msgKey && sessionStorage.getItem(msgKey) === "1") {
      // Important: clear any previous banner (e.g., "Checking...") so the UI isn't stuck
      $box.empty();
      return;
    }

    if (!kind) {
      // Plain text, no alert chrome
      $box.html('<div class="py-2 mb-0"></div>');
      $box.find("div").text(text);
      return;
    }

    if (!isDismissible) {
      // Non-dismissible (prevents the "stuck on checking" scenario)
      const cls = "alert alert-" + kind + " py-2 mb-0";
      $box.html('<div class="' + cls + '" role="alert"></div>');
      $box.find("div").text(text);
      return;
    }

    // Dismissible warning/danger, close button top-right
    const cls = "alert alert-" + kind + " alert-dismissible fade show py-2 mb-0 position-relative";
    $box.html(`
      <div class="${cls}" role="alert">
        <div class="me-4"></div>
        <button type="button"
                class="btn-close position-absolute top-0 end-0 mt-2 me-2"
                aria-label="Close"
                style="transform: scale(0.85);"></button>
      </div>
    `);

    const $alert = $box.find(".alert");
    $alert.find("div.me-4").text(text);

    // Remember on close (Bootstrap if available, fallback otherwise)
    $alert.on("closed.bs.alert", function () {
      sessionStorage.setItem(msgKey, "1");
    });

    $alert.find(".btn-close").on("click", function () {
      try {
        if (window.bootstrap && window.bootstrap.Alert) {
          window.bootstrap.Alert.getOrCreateInstance($alert[0]).close();
          return;
        }
      } catch (e) { /* ignore */ }

      // Fallback: manual close + remember
      sessionStorage.setItem(msgKey, "1");
      $alert.remove();
    });
  }

  function checkForNewFirmware() {
    if (!window.RelayBoardConfig) return;

    const curVer = (window.RelayBoardConfig.swVersionNum || "").trim();
    const curBuild = normalizeBuildNum(window.RelayBoardConfig.swBuildNum);

    const otaUrl = ($("#ota_update_url").val() || "").trim();
    if (!otaUrl) {
      renderOtaMsg("", null);
      return;
    }

    const buildInfoUrl = deriveBuildInfoUrl(otaUrl);
    if (!buildInfoUrl) {
      renderOtaMsg("OTA URL is not a valid URL (can't check for updates).", "warning");
      return;
    }

    renderOtaMsg("Checking for updates...", "info");

    // Cache buster to avoid stale JSON (S3/CDN/browser caching)
    const reqUrl = buildInfoUrl + "?_ts=" + Date.now();

    $.ajax({
      url: reqUrl,
      method: "GET",
      dataType: "json",
      timeout: 10000
    })
      .done(function (info) {
        const remoteVer = (info && (info.DEVICE_SW_VERSION_NUM || info.version_num || info.version)) ? String(info.DEVICE_SW_VERSION_NUM || info.version_num || info.version).trim() : "";
        const remoteBuild = normalizeBuildNum(info && (info.DEVICE_SW_BUILD_NUM || info.build_num || info.build) ? (info.DEVICE_SW_BUILD_NUM || info.build_num || info.build) : "");

        if (!remoteVer || !remoteBuild) {
          renderOtaMsg("build_info.json is missing DEVICE_SW_VERSION_NUM / DEVICE_SW_BUILD_NUM.", "warning");
          return;
        }

        window.latestOtaInfo = {
            version: remoteVer,
            build: remoteBuild,
            versionFull: info.DEVICE_SW_VERSION || (remoteVer + " build " + remoteBuild)
        };

        // Compare version first
        const vcmp = compareSemver(curVer, remoteVer);

        let isNewer = false;
        if (vcmp < 0) {
          isNewer = true;
        } else if (vcmp === 0) {
          // versions equal -> compare build numbers
          if (curBuild && remoteBuild && curBuild < remoteBuild) {
            isNewer = true;
          }
        }

        if (isNewer) {
          const niceRemote = info.DEVICE_SW_VERSION || (remoteVer + " build " + remoteBuild);

          const $box = ensureOtaMsgContainer();
          $box.html(`
            <div class="alert alert-warning alert-dismissible fade show py-2 mb-0 d-flex align-items-center justify-content-between" role="alert">
              
              <div>
                <b>New firmware available:</b> ${niceRemote}
                ${getUpdateButtonHtml()}
              </div>

              <button type="button" class="btn-close ms-3" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>
          `);
        } else {
          const niceCur = (curVer ? (curVer + (curBuild ? (" build " + curBuild) : "")) : "current");
          renderOtaMsg("Up to date (" + niceCur + ").", "success");
        }
      })
      .fail(function (xhr, status) {
        // Most common reason here (if request reaches browser) is CORS blocked by S3.
        renderOtaMsg("Update check failed (" + status + "). If OTA files are on another domain, ensure S3 CORS allows this UI origin.", "danger");
      });
  }

  $(function () {
    // Initial check after settings are loaded (your page loads settings via AJAX).
    // Give it a moment so ota_update_url is populated by /api/setting/get/all.
    setTimeout(checkForNewFirmware, 800);

    // Re-check when user edits OTA URL
    $(document).on("change blur", "#ota_update_url", function () {
      checkForNewFirmware();
    });
  });
})();

function submitOtaUpdate() {

  if (!window.latestOtaInfo) {
    alert("No update information available.");
    return;
  }

  // confirmation (optional but recommended)
  if (!confirm("Start firmware update? Device will reboot automatically.")) {
    return;
  }

  // create virtual form
  const form = document.createElement("form");
  form.method = "POST";
  form.action = "/ota-update";

  // --- current version ---
  const curVer = document.createElement("input");
  curVer.type = "hidden";
  curVer.name = "current_sw_version";
  curVer.value = window.RelayBoardConfig?.swVersionNum || "";
  form.appendChild(curVer);

  const curBuild = document.createElement("input");
  curBuild.type = "hidden";
  curBuild.name = "current_sw_build";
  curBuild.value = window.RelayBoardConfig?.swBuildNum || "";
  form.appendChild(curBuild);

  // --- NEW version (important) ---
  const newVer = document.createElement("input");
  newVer.type = "hidden";
  newVer.name = "new_sw_version";
  newVer.value = window.latestOtaInfo.versionFull;   // <-- see below
  form.appendChild(newVer);

  const newVerNum = document.createElement("input");
  newVerNum.type = "hidden";
  newVerNum.name = "new_sw_version_num";
  newVerNum.value = window.latestOtaInfo.version;     // e.g. "1.0.6"
  form.appendChild(newVerNum);

  const newBuildNum = document.createElement("input");
  newBuildNum.type = "hidden";
  newBuildNum.name = "new_sw_build_num";
  newBuildNum.value = window.latestOtaInfo.build;     // e.g. "20260203234334"
  form.appendChild(newBuildNum);

  document.body.appendChild(form);
  form.submit();
};

$(document).on("click", "#otaUpdateBtn", function () {
  submitOtaUpdate();
});

$(document).on("click", "#otaUpdateBtnStandalone", function () {
  submitOtaUpdate();
});