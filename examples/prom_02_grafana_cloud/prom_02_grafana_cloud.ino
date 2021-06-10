#include "config_test.h"
#include "certificates.h"
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>

PromLokiTransport transport;
PromClient client(transport);

// Create a write request for 2 series.
WriteRequest req(2,1024);

// Check out https://prometheus.io/docs/practices/naming/ for metric naming and label conventions.
// This library does not currently create different metric types like gauges, counters, and histograms
// however internally Prometheus does not differentiate between these types, rather they are both
// a naming convention and a usage convention so it's possible to create any of these yourself.
// See the README at https://github.com/grafana/prometheus-arduino for more info.

// Define a TimeSeries which can hold up to 5 samples, has a name of `uptime_milliseconds`
TimeSeries ts1(5, "uptime_milliseconds_total", "job=\"esp32-test\",host=\"esp32\"");

// Define a TimeSeries which can hold up to 5 samples, has a name of `heap_free_bytes`
TimeSeries ts2(5, "heap_free_bytes", "job=\"esp32-test\",host=\"esp32\",foo=\"bar\"");

// Note, metrics with the same name and different labels are actually different series and you would need to define them separately
//TimeSeries ts2(5, "heap_free_bytes", "job=\"esp32-test\",host=\"esp32\",foo=\"bar\"");

int loopCounter = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    Serial.println("Starting");
    Serial.print("Free Mem Before Setup: ");
    Serial.println(freeMemory());

    // Configure and start the transport layer
    transport.setUseTls(true);
    transport.setCerts(grafanaCert, strlen(grafanaCert));
    transport.setWifiSsid(WIFI_SSID);
    transport.setWifiPass(WIFI_PASSWORD);
    transport.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!transport.begin()) {
        Serial.println(transport.errmsg);
        while (true) {};
    }

    // Configure the client
    client.setUrl(GC_URL);
    client.setPath(GC_PATH);
    client.setPort(GC_PORT);
    client.setUser(GC_USER);
    client.setPass(GC_PASS);
    client.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!client.begin()) {
        Serial.println(client.errmsg);
        while (true) {};
    }

    // Add our TimeSeries to the WriteRequest
    req.addTimeSeries(ts1);
    req.addTimeSeries(ts2);
    req.setDebug(Serial);  // Remove this line to disable debug logging of the write request serialization and compression.

    Serial.print("Free Mem After Setup: ");
    Serial.println(freeMemory());

};



void loop() {
    int64_t time;
    time = transport.getTimeMillis();
    Serial.println(time);

    // Efficiency in requests can be gained by batching writes so we accumulate 5 samples before sending.
    // This is not necessary however, especially if your writes are infrequent, but it's recommended to build batches when you can.
    if (loopCounter >= 5) {
        //Send
        loopCounter = 0;
        PromClient::SendResult res = client.send(req);
        if (!res == PromClient::SendResult::SUCCESS) {
            Serial.println(client.errmsg);
            // Note: additional retries or error handling could be implemented here.
            // the result could also be:
            // PromClient::SendResult::FAILED_DONT_RETRY
            // PromClient::SendResult::FAILED_RETRYABLE
        }
        // Batches are not automatically reset so that additional retry logic could be implemented by the library user.
        // Reset batches after a succesful send.
        ts1.resetSamples();
        ts2.resetSamples();
    }
    else {
        if (!ts1.addSample(time, millis())) {
            Serial.println(ts1.errmsg);
        }
        if (!ts2.addSample(time, freeMemory())) {
            Serial.println(ts2.errmsg);
        }
        loopCounter++;
    }

    delay(500);

};