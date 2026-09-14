function formatTimeSinceBoot(microseconds) {
    let total_seconds = Math.floor(microseconds / 1000000); // Convert microseconds to seconds
    
    let days = Math.floor(total_seconds / (24 * 60 * 60)); // Calculate days
    total_seconds %= (24 * 60 * 60); // Get remaining seconds after calculating days
    
    let hours = Math.floor(total_seconds / (60 * 60)); // Calculate hours
    total_seconds %= (60 * 60); // Get remaining seconds after calculating hours
    
    let minutes = Math.floor(total_seconds / 60); // Calculate minutes
    let seconds = total_seconds % 60; // Remaining seconds

    return `${days} days ${hours} hours ${minutes} minutes ${seconds} seconds`;
}

function updateSensorData() {
    $.ajax({
        url: '/api/status',
        type: 'GET',
        dataType: 'json',
        success: function(response) {
            $('#val_pressure').text(response.sensor.pressure.toFixed(2));
            $('#val_pressure_atm').text((response.sensor.pressure / 101325).toFixed(2));
            $('#val_voltage').text(response.sensor.voltage.toFixed(3));
            $('#val_voltage_offset').text(response.sensor.voltage_offset.toFixed(3));
            $('#val_sensor_linear_multiplier').text(response.sensor.sensor_linear_multiplier);
            $('#val_voltage_raw').text(response.sensor.voltage_raw);
            $('#val_sampling_enabled').text(response.status.sensor_sampling_enabled == 1 ? "Enabled" : "Disabled");

            $('#val_free_heap').text(response.status.free_heap);
            $('#val_min_free_heap').text(response.status.min_free_heap);
            // Convert time since boot to a readable format and update the element
            let time_since_boot = formatTimeSinceBoot(response.status.time_since_boot);
            $('#val_time_since_boot').text(time_since_boot);
            if (response.status.memguard_threshold !== undefined && response.status.memguard_mode !== undefined) {
                if (response.status.memguard_mode > 0) {
        
                    $("#val_memguard_threshold").text(response.status.memguard_threshold);
                    // unhide the memguard row
                    $("#row_memguard_threshold").css("display", "table-row");
                } else {
                    // hide the memguard row
                    $("#row_memguard_threshold").css("display", "none");
                }
            }
            
        },
        error: function() {
            console.error("Failed to fetch sensor data");
        }
    });
}