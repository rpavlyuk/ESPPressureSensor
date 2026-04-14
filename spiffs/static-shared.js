// script-common.js

// 1) Generic helpers (global, so HTML can call them)
function confirmReset(message) {
  return confirm(message);
}

function selectElement(id, valueToSelect) {
  const element = document.getElementById(id);
  if (!element) return;
  element.value = valueToSelect;
}