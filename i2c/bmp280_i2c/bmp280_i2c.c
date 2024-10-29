/**
 * Copyright (c) 2023 Milk-V
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **/

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <wiringx.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>

//define port 8080
#define PORT 8080

// depends on which port to use
#define I2C_DEV "/dev/i2c-1"

// device has default bus address of 0x76
#define I2C_ADDR  0x76

// hardware registers
#define REG_CONFIG    0xF5
#define REG_CTRL_MEAS 0xF4
#define REG_CTRL_HUM  0xF2
#define REG_RESET     0xE0

#define REG_TEMP_XLSB 0xFC
#define REG_TEMP_LSB  0xFB
#define REG_TEMP_MSB  0xFA

#define REG_PRESSURE_XLSB 0xF9
#define REG_PRESSURE_LSB  0xF8
#define REG_PRESSURE_MSB  0xF7

#define REG_HUM_LSB   0xFE
#define REG_HUM_MSB   0xFD

// calibration registers
#define REG_DIG_T1 0x88
#define REG_DIG_T2 0x8A
#define REG_DIG_T3 0x8C

#define REG_DIG_P1 0x8E
#define REG_DIG_P2 0x90
#define REG_DIG_P3 0x92
#define REG_DIG_P4 0x94
#define REG_DIG_P5 0x96
#define REG_DIG_P6 0x98
#define REG_DIG_P7 0x9A
#define REG_DIG_P8 0x9C
#define REG_DIG_P9 0x9E

#define REG_DIG_H1 0xA1
#define REG_DIG_H2 0xE1
#define REG_DIG_H3 0xE3
#define REG_DIG_H4 0xE4
#define REG_DIG_H5 0xE5
#define REG_DIG_H6 0xE7

void bme280_init(int fd);



/*
 * Immutable calibration data read from BME280
 */
struct bme280_calib_param {
    // temperature params
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    // pressure params
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;

    // humidity params
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
};

void bme280_read_raw(int fd, int32_t* temp, int32_t* pressure, int32_t* humidity) {
    uint8_t buf[8];

    wiringXI2CWrite(fd, REG_PRESSURE_MSB);
    buf[0] = wiringXI2CRead(fd);
    buf[1] = wiringXI2CRead(fd);
    buf[2] = wiringXI2CRead(fd);
    buf[3] = wiringXI2CRead(fd);
    buf[4] = wiringXI2CRead(fd);
    buf[5] = wiringXI2CRead(fd);
    buf[6] = wiringXI2CRead(fd);
    buf[7] = wiringXI2CRead(fd);

    // store the 20-bit read in a 32-bit signed integer for conversion
    *pressure = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
    *temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
    *humidity = (buf[6] << 8) | buf[7];
}

struct bme280_data {
    char temperature[100];
    char pressure[100];
    char humidity[100];
};

struct bme280_data* read_bme280_data() {
    int fd_i2c;
    struct bme280_data* data = (struct bme280_data*)malloc(sizeof(struct bme280_data));
    if (data == NULL) {
        return NULL; // Handle memory allocation failure
    }

    if (wiringXSetup("duo", NULL) == -1) {
        wiringXGC();
        free(data);
        return NULL;
    }

    if ((fd_i2c = wiringXI2CSetup(I2C_DEV, I2C_ADDR)) < 0) {
        free(data);
        return NULL;
    }

    bme280_init(fd_i2c);

    struct bme280_calib_param params;
    bme280_get_calib_params(fd_i2c, &params);

    usleep(250000);

    int32_t raw_temperature;
    int32_t raw_pressure;
    int32_t raw_humidity;

    bme280_read_raw(fd_i2c, &raw_temperature, &raw_pressure, &raw_humidity);
    int32_t temperature = bme280_convert_temp(raw_temperature, &params);
    int32_t pressure = bme280_convert_pressure(raw_pressure, raw_temperature, &params);
    int32_t humidity = bme280_convert_humidity(raw_humidity, &params);

    snprintf(data->temperature, 100, " = %.2f C", temperature / 100.f);
    snprintf(data->pressure, 100, " = %.2f hPa", pressure / 25600.f); // Modified line to show pressure in hPa
    snprintf(data->humidity, 100, " = %.2f %%", humidity / 1024.f);

    return data;
}

void bme280_init(int fd) {
    // use the "handheld device dynamic" optimal setting (see datasheet)

    // Set humidity oversampling to x1
    wiringXI2CWriteReg8(fd, REG_CTRL_HUM, 0x01);

    // 500ms sampling time, x16 filter
    const uint8_t reg_config_val = ((0x04 << 5) | (0x05 << 2)) & 0xFC;
    wiringXI2CWriteReg8(fd, REG_CONFIG, reg_config_val);

    // osrs_t x1, osrs_p x4, normal mode operation
    const uint8_t reg_ctrl_meas_val = (0x01 << 5) | (0x03 << 2) | (0x03);
    wiringXI2CWriteReg8(fd, REG_CTRL_MEAS, reg_ctrl_meas_val);
}

void bme280_get_calib_params(int fd, struct bme280_calib_param* params) {
    // raw temp, pressure, and humidity values need to be calibrated
    params->dig_t1 = (uint16_t)wiringXI2CReadReg16(fd, REG_DIG_T1);
    params->dig_t2 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_T2);
    params->dig_t3 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_T3);

    params->dig_p1 = (uint16_t)wiringXI2CReadReg16(fd, REG_DIG_P1);
    params->dig_p2 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P2);
    params->dig_p3 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P3);
    params->dig_p4 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P4);
    params->dig_p5 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P5);
    params->dig_p6 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P6);
    params->dig_p7 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P7);
    params->dig_p8 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P8);
    params->dig_p9 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_P9);

    params->dig_h1 = wiringXI2CReadReg8(fd, REG_DIG_H1);
    params->dig_h2 = (int16_t)wiringXI2CReadReg16(fd, REG_DIG_H2);
    params->dig_h3 = wiringXI2CReadReg8(fd, REG_DIG_H3);

    int16_t h4_msb = wiringXI2CReadReg8(fd, REG_DIG_H4);
    int16_t h4_lsb = wiringXI2CReadReg8(fd, REG_DIG_H4 + 1);
    params->dig_h4 = (h4_msb << 4) | (h4_lsb & 0x0F);

    int16_t h5_msb = wiringXI2CReadReg8(fd, REG_DIG_H5 + 1);
    int16_t h5_lsb = wiringXI2CReadReg8(fd, REG_DIG_H5);
    params->dig_h5 = (h5_msb << 4) | (h5_lsb >> 4);

    params->dig_h6 = (int8_t)wiringXI2CReadReg8(fd, REG_DIG_H6);
}

int32_t t_fine;

int32_t bme280_convert_temp(int32_t temp, struct bme280_calib_param* params) {
    int32_t var1, var2;

    var1 = ((((temp >> 3) - ((int32_t)params->dig_t1 << 1))) *
            ((int32_t)params->dig_t2)) >> 11;

    var2 = (((((temp >> 4) - ((int32_t)params->dig_t1)) *
              ((temp >> 4) - ((int32_t)params->dig_t1))) >> 12) *
            ((int32_t)params->dig_t3)) >> 14;

    t_fine = var1 + var2;

    return (t_fine * 5 + 128) >> 8;
}

int32_t bme280_convert_pressure(int32_t pres, int32_t temp, struct bme280_calib_param* params) {
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)params->dig_p6;
    var2 = var2 + ((var1 * (int64_t)params->dig_p5) << 17);
    var2 = var2 + (((int64_t)params->dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t)params->dig_p3) >> 8) +
           ((var1 * (int64_t)params->dig_p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)params->dig_p1) >> 33;

    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }

    p = 1048576 - pres;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)params->dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)params->dig_p8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)params->dig_p7) << 4);
    
    // hPa conversion (pressure in Pa / 100.0)
    return (int32_t)(p / 1); // pressure is scaled up by 1
}

int32_t bme280_convert_humidity(int32_t hum, struct bme280_calib_param* params) {
    int32_t v_x1_u32r;

    v_x1_u32r = (t_fine - ((int32_t)76800));

    v_x1_u32r =
        (((((hum << 14) - (((int32_t)params->dig_h4) << 20) -
            (((int32_t)params->dig_h5) * v_x1_u32r)) +
           ((int32_t)16384)) >>
          15) *
         (((((((v_x1_u32r * ((int32_t)params->dig_h6)) >> 10) *
              (((v_x1_u32r * ((int32_t)params->dig_h3)) >> 11) +
               ((int32_t)32768))) >>
             10) +
            ((int32_t)2097152)) *
               ((int32_t)params->dig_h2) +
           8192) >>
          14));

    v_x1_u32r =
        (v_x1_u32r -
         (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
           ((int32_t)params->dig_h1)) >>
          4));

    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

    return (uint32_t)(v_x1_u32r >> 12);
}

int main() {

    
    struct bme280_data* data = read_bme280_data();
    if (data != NULL) {
        printf("Temperature%s\n", data->temperature);
        printf("Pressure%s\n", data->pressure);
        printf("Humidity%s\n", data->humidity);
        
        free(data);
    } else {
        printf("Failed to read data from BME280 sensor.\n");
           }
           sleep(30);
    

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    address.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Accept and handle incoming connections
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Handle the client in a separate function
        //handleClient(new_socket);

	char buffer[30000] = {0};
    read(new_socket, buffer, 30000);

    printf("Received request:\n%s\n", buffer);



    /*struct bme280_data* data1 = read_bme280_data();
    if (data1 != NULL) {
        printf("Temperature%s\n", data1->temperature);
        printf("Pressure%s\n", data1->pressure);
        printf("Humidity%s\n", data1->humidity);
    } else {
        printf("Failed to read data from BME280 sensor.\n");
        return 1;
    }*/
    

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Accept and handle incoming connections
    while (1) {

        struct bme280_data* data1 = read_bme280_data();
           if (data1 != NULL) {
              printf("Temperature%s\n", data1->temperature);
              printf("Pressure%s\n", data1->pressure);
              printf("Humidity%s\n", data1->humidity);
           } else {
               printf("Failed to read data from BME280 sensor.\n");
        return 1;
                             }

        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Buffer to read the incoming request
        char buffer[30000] = {0};
        read(new_socket, buffer, 30000);

        printf("Received request:\n%s\n", buffer);

        // Prepare the HTML response with temperature, pressure, and humidity
        char httpResponse[1024];
        snprintf(httpResponse, sizeof(httpResponse),
                 "HTTP/1.1 200 OK\n"
                 "Content-Type: text/html\n\n"
                 "<html><body>"
                 "<h1>Welcome to Milkv Duo Web Server</h1>"
                 "<p>Temperature%s</p>"
                 "<p>Pressure%s</p>"
                 "<p>Humidity%s</p>"
                 "</body></html>",
                 data1->temperature, data1->pressure, data1->humidity);

        // Send the HTTP response
        send(new_socket, httpResponse, strlen(httpResponse), 0);
        printf("Response sent\n");

        // Close the socket for this client
        close(new_socket);

        // Free the allocated memory for sensor data
         free(data1);
    }

    
    
    return 0;
}
}