
void ndp_setup() {
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpLocalTime);
  setSyncInterval(3600);
}
