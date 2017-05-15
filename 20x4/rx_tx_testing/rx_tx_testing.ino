void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("hello");
  delay(1000);
  if(Serial.available() > 0)
  {
    Serial.println("1");
    Serial.println(Serial.read());
    
  }

}
