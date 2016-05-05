#include "passthru_shadow.h"

bool messageArrivedOnDelta = false;

void passthru_shadow_connect(passthru_shadow *shadow) {

  char errmsg[255];
  char rootCA[255];
  char clientCRT[255];
  char clientKey[255];
  char CurrentWD[255];
  char certDirectory[10] = "certs";
  char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
  char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
  char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;
  shadow->rc = NONE_ERROR;

  getcwd(CurrentWD, sizeof(CurrentWD));
  sprintf(rootCA, "%s/%s/%s", CurrentWD, certDirectory, cafileName);
  sprintf(clientCRT, "%s/%s/%s", CurrentWD, certDirectory, clientCRTName);
  sprintf(clientKey, "%s/%s/%s", CurrentWD, certDirectory, clientKeyName);

  syslog(LOG_DEBUG, "rootCA %s", rootCA);
  syslog(LOG_DEBUG, "clientCRT %s", clientCRT);
  syslog(LOG_DEBUG, "clientKey %s", clientKey);

  MQTTClient_t mqttClient;
  aws_iot_mqtt_init(&mqttClient);
  shadow->mqttClient = &mqttClient;

  ShadowParameters_t sp = ShadowParametersDefault;
  sp.pMyThingName = AWS_IOT_MY_THING_NAME;
  sp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
  sp.pHost = AWS_IOT_MQTT_HOST;
  sp.port = AWS_IOT_MQTT_PORT;
  sp.pClientCRT = clientCRT;
  sp.pClientKey = clientKey;
  sp.pRootCA = rootCA;

  MQTTwillOptions lwt;
  lwt.pTopicName = "ecutools/lwt";
  lwt.pMessage = "\{connected\": \"false\"}";
  lwt.qos = QOS_1;

  shadow->rc = aws_iot_shadow_init(shadow->mqttClient);
  if(shadow->rc != NONE_ERROR) {
    sprintf(errmsg, "aws_iot_shadow_init error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
    return;
  }

  shadow->rc = aws_iot_shadow_connect(shadow->mqttClient, &sp);
  if(shadow->rc != NONE_ERROR) {
    sprintf(errmsg, "aws_iot_shadow_connect error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
    return;
  }
/*
  shadow->rc = shadow->mqttClient->setAutoReconnectStatus(true);
  if(shadow->rc != NONE_ERROR){
    sprintf(errmsg, "setAutoReconnectStatus error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
    return;
  }
*/
  jsonStruct_t deltaObject;
  deltaObject.pData = DELTA_REPORT;
  deltaObject.pKey = "state";
  deltaObject.type = SHADOW_JSON_OBJECT;
  deltaObject.cb = shadow->ondelta;

  shadow->rc = aws_iot_shadow_register_delta(shadow->mqttClient, &deltaObject);
  if(shadow->rc != NONE_ERROR) {
    sprintf(errmsg, "aws_iot_shadow_register_delta error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
    return;
  }

  shadow->onopen(shadow);
}

void passthru_shadow_report_delta(passthru_shadow *shadow) {
  syslog(LOG_DEBUG, "Sending delta report: %s", DELTA_REPORT);
  shadow->rc = aws_iot_shadow_update(shadow->mqttClient, AWS_IOT_MY_THING_NAME, DELTA_REPORT, shadow->onupdate, NULL, 2, true);
  if(shadow->rc != NONE_ERROR) {
    char errmsg[255];
    sprintf(errmsg, "aws_iot_shadow_update error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
  }
}

void passthru_shadow_get(passthru_shadow *shadow) {
  syslog(LOG_DEBUG, "passthru_shadow_get: %s", DELTA_REPORT);
  shadow->rc = aws_iot_shadow_get(shadow->mqttClient, AWS_IOT_MY_THING_NAME, shadow->onget, NULL, 2, true);
  if(shadow->rc != NONE_ERROR) {
    char errmsg[255];
    sprintf(errmsg, "aws_iot_shadow_get error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
  }
}

void passthru_shadow_update(passthru_shadow *shadow, char *message) {
  syslog(LOG_DEBUG, "passthru_shadow_update: %s", message);
  shadow->rc = aws_iot_shadow_update(shadow->mqttClient, AWS_IOT_MY_THING_NAME, message, shadow->onupdate, NULL, 2, true);
  if(shadow->rc != NONE_ERROR) {
    char errmsg[255];
    sprintf(errmsg, "aws_iot_shadow_update error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
  }
}

void passthru_shadow_disconnect(passthru_shadow *shadow) {
  syslog(LOG_DEBUG, "passthru_shadow_disconnect");
  shadow->rc = aws_iot_shadow_disconnect(shadow->mqttClient);
  if(shadow->rc != NONE_ERROR) {
    char errmsg[255];
    sprintf(errmsg, "aws_iot_shadow_disconnect error rc=%d", shadow->rc);
    shadow->onerror(shadow, errmsg);
  }
}

bool passthru_shadow_build_report_json(char *pJsonDocument, size_t maxSizeOfJsonDocument, const char *pData, uint32_t pDataLen) {

  int32_t ret;

  if (pJsonDocument == NULL) {
    return false;
  }

  char tempClientTokenBuffer[MAX_SIZE_CLIENT_TOKEN_CLIENT_SEQUENCE];

  if(aws_iot_fill_with_client_token(tempClientTokenBuffer, MAX_SIZE_CLIENT_TOKEN_CLIENT_SEQUENCE) != NONE_ERROR){
    return false;
  }

  ret = snprintf(pJsonDocument, maxSizeOfJsonDocument, "{\"state\":{\"reported\":%.*s}, \"clientToken\":\"%s\"}", pDataLen, pData, tempClientTokenBuffer);

  if (ret >= maxSizeOfJsonDocument || ret < 0) {
    return false;
  }

  return true;
}