module GreenhouseMonitor_tb;

    reg clk;
    reg rst;
    reg [7:0] temperature;
    reg [7:0] humidity;
    reg [7:0] soil_moisture;
    reg [7:0] co2_level;
    reg [7:0] light_intensity;
    reg [7:0] pressure;
    reg [7:0] ph_level;
    reg [7:0] pest_level;
    reg [7:0] leaf_color_in;
    reg remote_fan;
    reg remote_irrigation;
    reg remote_humidity_control;

    wire fan;
    wire irrigation;
    wire humidity_control;
    wire alert;
    wire [7:0] growth_status;
    wire [7:0] leaf_health_out;

    integer file;

    GreenhouseMonitor uut (
        .clk(clk),
        .rst(rst),
        .temperature(temperature),
        .humidity(humidity),
        .soil_moisture(soil_moisture),
        .co2_level(co2_level),
        .light_intensity(light_intensity),
        .pressure(pressure),
        .ph_level(ph_level),
        .pest_level(pest_level),
        .leaf_color_in(leaf_color_in),
        .remote_fan(remote_fan),
        .remote_irrigation(remote_irrigation),
        .remote_humidity_control(remote_humidity_control),
        .fan(fan),
        .irrigation(irrigation),
        .humidity_control(humidity_control),
        .alert(alert),
        .growth_status(growth_status),
        .leaf_health_out(leaf_health_out) 
    );

    always #5 clk = ~clk;

    initial begin
        clk = 0;
        rst = 1;
        temperature = 8'd25;
        humidity = 8'd50;
        soil_moisture = 8'd50;
        co2_level = 8'd40;
        light_intensity = 8'd60;
        pressure = 8'd101;
        ph_level = 8'd60;
        pest_level = 8'd10;
        leaf_color_in = 8'd80;
        remote_fan = 1;
        remote_irrigation = 1;
        remote_humidity_control = 1;

        #10 rst = 0;

        file = $fopen("sensor_data.txt", "w");
        if (file == 0) begin
            $display("Error: Unable to open file");
            $finish;
        end

        repeat (150) begin
            #20 temperature = temperature + 1;
            #20 humidity = humidity - 1;
            #20 soil_moisture = soil_moisture - 2;
            #20 co2_level = co2_level + 5;
            #20 light_intensity = light_intensity - 3;
            #20 pest_level = pest_level + 5; 
            #20 ph_level = ph_level - 1;
            #20 leaf_color_in = {1'b0, $random % 101}; 
            if (leaf_color_in < 0) leaf_color_in = 0;
            if (leaf_color_in > 100) leaf_color_in = 100;

            $fwrite(file, "Temperature: %d, Humidity Level: %d, Soil Moisture: %d, CO2 Level: %d, Light Intensity: %d, Pressure: %d, pH level: %d, Pest Level: %d, Leaf health(direct): %d, Leaf health(refined): %d, Fan Status: %d, Irrigation: %d, Humidity Control: %d, Alert: %d\n",
                    temperature, humidity, soil_moisture, co2_level, light_intensity, pressure, ph_level, pest_level, leaf_color_in, uut.leaf_health_out, fan, irrigation, humidity_control, alert);
        end

        $fclose(file);
        $finish;
    end

    initial begin
        $monitor("Time: %0t | Temp: %0d | Humidity: %0d | Soil Moist: %0d | CO2: %0d | Light: %0d | Pressure: %0d | pH: %0d | Pest: %0d | Leaf In: %0d | Leaf Health (Refined): %0d | Fan: %s | Irrigation: %s | Humidity Ctrl: %s | Alert: %0d | Growth: %b\n",
                 $time, temperature, humidity, soil_moisture, co2_level, light_intensity, pressure, ph_level, pest_level, leaf_color_in, uut.leaf_health_out,
                 (uut.fan == 1) ? "ON" : "OFF",
                 (uut.irrigation == 1) ? "ON" : "OFF",
                 (uut.humidity_control == 1) ? "ON" : "OFF",
                 uut.alert, uut.growth_status);
    end

endmodule
