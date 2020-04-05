#include "mbed.h"
#include "LSM6DSLSensor.h"
#include <MQTTClientMbedOs.h>

static DevI2C devI2c(PB_11,PB_10);
static LSM6DSLSensor acc_gyro(&devI2c,LSM6DSL_ACC_GYRO_I2C_ADDRESS_LOW,PD_11); // low address
static DigitalOut shutdown_pin(PC_6);
volatile int mems_event = 0;
volatile int toggle_free_fall_enable = 0;
static int free_fall_is_enabled = 1;

InterruptIn mybutton(USER_BUTTON);
DigitalOut myled(LED1);
WiFiInterface *wifi;
TCPSocket* socket;
MQTTClient* mqttClient;
Thread t;
EventQueue queue(5 * EVENTS_EVENT_SIZE);

const char CLIENT_ID[] = "2791a0dd-0ea8-45ba-b785-245c43934aa4";
const char NETPIE_TOKEN[] = "aZuLnToAjJu8WhjxAETc9eNsToxAxijG"; 
const char NETPIE_SECRET[] = "AG-vql&^WiVYE(hON5%Jn90XD.epp4wK";
const char MQTT_TOPIC[] = "@msg/Fall Detected!";

/* User button callback. */
void pressed_cb() {
  toggle_free_fall_enable = 1;
}
 
/* Interrupt 1 callback. */
void int1_cb() {
  mems_event = 1;
}

/* main function */
int main() {

  // WiFi connection
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    // Socket connection
    socket = new TCPSocket();
    socket->open(wifi);
    SocketAddress addr;
    wifi->gethostbyname("mqtt.netpie.io", &addr);
    addr.set_port(1883);
    socket->connect(addr);
    if (ret != 0) {
        printf("rc from TCP connect is %d\r\n", ret);
        return -1;
    }

    // MQTT connection
    mqttClient = new MQTTClient(socket); 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    //data.MQTTVersion = 3;
    data.clientID.cstring = (char*)CLIENT_ID;
    data.username.cstring = (char*)NETPIE_TOKEN;
    //data.password.cstring = (char*)NETPIE_SECRET;
    ret = mqttClient->connect(data);
    if (ret != 0) {
        printf("rc from MQTT connect is %d\r\n", ret);
        return -1;
    }

	//sensor
    mybutton.fall(&pressed_cb);
    acc_gyro.attach_int1_irq(&int1_cb);
  
    int32_t axes[3];
    acc_gyro.init(NULL);
    acc_gyro.enable_x();
    acc_gyro.enable_free_fall_detection();
  
    while(1) {
      if(toggle_free_fall_enable) {
        toggle_free_fall_enable = 0;
        if(free_fall_is_enabled == 0) {
          acc_gyro.enable_free_fall_detection();
          free_fall_is_enabled = 1;
        } else {
          acc_gyro.disable_free_fall_detection();
          free_fall_is_enabled = 0;
        }
      }
 
      if (mems_event) {
        mems_event = 0;
        LSM6DSL_Event_Status_t status;
        acc_gyro.get_event_status(&status);
        if (status.FreeFallStatus) {
        /* Output data. */
          printf("Fall Detected!\r\n");
	/* Led blinking. */
          myled = 1;
          wait(2);
          myled = 0;

MQTT::Message message;
 
    // QoS 0
    char buf[100];
    sprintf(buf, "Fall");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
 printf("Sending MQTT message\n");
    ret = mqttClient->publish(MQTT_TOPIC, message);
    if (ret != 0) {
        printf("rc from publish was %d\r\n", ret);
    }
        }
      }
    }
}
