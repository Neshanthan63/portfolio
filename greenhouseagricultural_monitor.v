module GreenhouseMonitor (
    input wire clk,
    input wire rst,
    input wire [7:0] temperature,
    input wire [7:0] humidity,
    input wire [7:0] soil_moisture,
    input wire [7:0] co2_level,
    input wire [7:0] light_intensity,
    input wire [7:0] pressure,
    input wire [7:0] ph_level,
    input wire [7:0] pest_level,
    input wire [7:0] leaf_color,
    input wire remote_fan,
    input wire remote_irrigation,
    input wire remote_humidity_control,
    output reg fan,
    output reg irrigation,
    output reg humidity_control,
    output reg alert,
    output reg [7:0] growth_status
);

always @(posedge clk or posedge rst) begin
    if (rst) begin
        fan <= 0;
        irrigation <= 0;
        humidity_control <= 0;
        alert <= 0;
        growth_status <= 8'b00000000;
    end else begin
        // Automatic Fan Control (Temp & CO2-based)
        if (temperature > 40 || co2_level > 100) begin
            fan <= 1;
        end
        if (remote_fan == 0) begin
            fan <= 0;
        end

        // Automatic Irrigation Control (Soil Moisture & pH-based)
        if (soil_moisture < 30 || ph_level < 50 || ph_level > 75) begin
            irrigation <= 1;
        end
        if (remote_irrigation == 0) begin
            irrigation <= 0;
        end

        // Humidity Control (Added)
        if (humidity < 40 || humidity > 70) begin
            humidity_control <= 1;
        end
        if (remote_humidity_control == 0) begin
            humidity_control <= 0;
        end

        // Pest and Leaf Color Alerts
        if (pest_level > 50 || leaf_color < 30) begin
            alert <= 1;
        end

        // Abnormal Condition Alerts
        if (temperature > 45 || co2_level > 120 || soil_moisture < 20 || light_intensity < 10 || humidity < 30) begin
            alert <= 1;
        end

        // Growth Prediction Logic
        if (temperature > 25 && temperature < 35 && soil_moisture > 40 && humidity > 50 && humidity < 70 && light_intensity > 50) begin
            growth_status <= 8'b11111111;
        end else if (temperature > 35 || soil_moisture < 40 || humidity < 50 || light_intensity < 50) begin
            growth_status <= 8'b01111111;
        end else begin
            growth_status <= 8'b00001111;
        end
    end
end

endmodule