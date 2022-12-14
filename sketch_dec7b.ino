#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

const int HX711_dout = 4; //Pin digital 4 -> 
const int HX711_sck = 5; //Pin digital 5 ->
const int JoyStick_X = A4; //Pin analog 4 -> variation du déplacement du joystick selon l'axe X (Horizontalement).
const int JoyStick_Y = A5; //Pin analog 5 -> variation du déplacement du joystick selon l'axe Y (Verticalement).
const int JoyStick_Bouton = 3; //Pin digital 2 -> Bouton de commande situé sur le joystick : utilisé dans le calibrage et l'enregistrement du pot témoin.
const int FinCourse_1 = 2; //Pin digital 3 -> Capteur de position maximale de la vanne.
const int Bouton_1 = 7; //Pin digital 7 -> Bouton de commande : lance un cycle complet de remplissage d'un pot.
const int Bouton_2 = 8; //Pin digital 8 -> Bouton de commande : utilisé dans le calibrage (à enlever pour optimiser).
boolean resume = false;

const int RPWM = 9; //Pin digital 9 -> contrôle la rentrée du vérin et l'ouverture de la vanne.
const int LPWM = 10; //Pin digital 10 -> contrôle la sortie du vérin et la fermeture de la vanne.

HX711_ADC LoadCell(HX711_dout, HX711_sck); //Étape de construction de l'HX711 de la cellule de charge.
const int calVal_eepromAdress = 0;
unsigned long t = 0;

float masse_1 = 0.0;
float masse_2 = 0.0;
float masse_3 = 0.0;
float masse_4 = 0.0;
float Deltamasse = 0.0;
float diffmasse = 0.0;

int Compteur = 0;
boolean cycle = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  delay(10);

  pinMode(JoyStick_Bouton, INPUT_PULLUP); //Défini la broche du JoyStick_Bouton (pin 2) comme une entrée.
  pinMode(FinCourse_1, INPUT_PULLUP); //Défini la broche du FinCourse_1 (pin 3) comme une entrée.
  pinMode(Bouton_1, INPUT_PULLUP); //Défini la broche du Bouton_1 (pin 7) comme une entrée.
  pinMode(Bouton_2, INPUT_PULLUP); //Défini la broche du Bouton_2 (pin 8) comme une entrée.

  attachInterrupt(digitalPinToInterrupt(2), fincourse1, RISING); //Défini une interruption par la pin 3 (broche du FinCourse_1) en flot montant (LOW -> HICH) sur la fonction "fincourse1".
  
  TCCR2A = 0; //default 
  TCCR2B = 0b00000110; // clk/256 est incrémenté toutes les 16uS  
  TIMSK2 = 0b00000001; // TOIE2 
  sei(); //autorise les interruptions

  Serial.println("Starting...");
  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    Serial.println("Délai expiré, vérifiez le câblage MCU -> HX711 et les désignations des broches");
    while (1);
  }
  else {
    LoadCell.setCalFactor(1.0);
    Serial.println("Le démarrage est terminé");
  }
  while (!LoadCell.update());
  calibrate();
}

void loop() {
  // put your main code here, to run repeatedly:
  static boolean newDataReady = false;
  const int serialPrintInterval = 0;

  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      newDataReady = 0;
      /*Serial.print("Poid : ");
      Serial.println(LoadCell.getData());*/
      t = millis();
    }
  }
  

  int b2 = digitalRead(Bouton_2);
  if (b2 == LOW) {
    LoadCell.tareNoDelay();
    masse_1 = 0.0;
    masse_2 = 0.0;
    Deltamasse = 0.0;
    cycle = true;
  }
  
  if (LoadCell.getTareStatus() == true && cycle == true) {
    Serial.println("Tare complète");
    analogWrite(LPWM, 0);
    analogWrite(RPWM, 250);
    }

  if (masse_1 == 0.0) {
    masse_1 = LoadCell.getData();
  }
  else {
    masse_2 = LoadCell.getData();
    Deltamasse = masse_2 - masse_1;
  }

  if (Deltamasse >= 2.0 && cycle == true) {
    Compteur = 1;
    cycle = false;
    masse_3 = LoadCell.getData();
  }

  if (Deltamasse >= 250 - diffmasse) {
      analogWrite(RPWM, 0);
      analogWrite(LPWM, 250);
  }
}

void fincourse1() 
{
  Serial.print("Fin de Course 1 : ");
  Serial.println(digitalRead(FinCourse_1));
  analogWrite(RPWM, 0);
  analogWrite(LPWM, 250);
}

ISR (TIMER2_OVF_vect) {
  TCNT2 = 6;
  if (Compteur > 0) {
    Serial.print("Compteur : ");
    Serial.println(Compteur);
    if (Compteur++ == 500) {
      Compteur = 0;
      analogWrite(RPWM, 0);
      masse_4 = LoadCell.getData();
      diffmasse = masse_4 - masse_3;
    }
  }
  
}

void calibrate() {
  Serial.println("***");
  Serial.println("Démarrer de la calibration :");
  Serial.println("Placez la cellule de charge sur une surface plane et stable.");
  Serial.println("Retirez toute charge appliquée à la cellule de charge.");
  Serial.println("Appuyez sur le Bouton du JoyStick pour régler le décalage de la tare.");

  resume = false;
  while (resume == false) {
    LoadCell.update();
    if (digitalRead(JoyStick_Bouton) == LOW) {
      LoadCell.tareNoDelay();
    }
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complète");
      resume = true;
    }
  }

  Serial.println("Maintenant, placez votre masse connue sur la cellule de charge.");
  Serial.println("Sélectionnez ensuite le poids de cette masse à partir du JoyStick selon l'axe Vertical.");
  Serial.println("Terminez en appuyant sur le Bouton du JoyStick.");

  float known_mass = 100.0;
  resume = false;
  while (resume == false) {
    LoadCell.update();
    int y = analogRead(JoyStick_Y);
    if (y < 146) {
      known_mass = known_mass - 1;
    }
    else if (y < 292) {
      known_mass = known_mass - 1;
    }
    else if (y < 438) {
      known_mass = known_mass - 1;
    }
    else if (y < 584) {

    }
    else if (y < 730) {
      known_mass = known_mass + 1;
    }
    else if (y < 876) {
      known_mass = known_mass + 1;
    }
    else {
      known_mass = known_mass + 1;
    }
    delay(50);
    Serial.print("Masse connue : ");
    Serial.print(known_mass);
    Serial.println(" g");

    if (digitalRead(JoyStick_Bouton) == LOW) {
      if (known_mass > 0) {
        Serial.print("Masse enregistrée : ");
        Serial.println(known_mass);
        resume = true;
      }
    }
  }

  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
  float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

  Serial.print("La nouvelle valeur d'étalonnage a été définie sur :");
  Serial.print(newCalibrationValue);
  Serial.println(", utilisez-la comme valeur d'étalonnage (calFactor) dans votre esquisse de projet.");
  Serial.print("Enregistrez cette valeur à l'adresse EEPROM : ");
  Serial.print(calVal_eepromAdress);
  Serial.println("? Bouton 1 pour oui / Bouton 2 pour non");

  resume = false;
  while (resume == false) {
    if (digitalRead(Bouton_1) == LOW) {
      #if defined(ESP8266)|| defined(ESP32)
        EEPROM.begin(512);
      #endif
        EEPROM.put(calVal_eepromAdress, newCalibrationValue);
      #if defined(ESP8266)|| defined(ESP32)
        EEPROM.commit();
      #endif
        EEPROM.get(calVal_eepromAdress, newCalibrationValue);
        Serial.print("Valeur ");
        Serial.print(newCalibrationValue);
        Serial.print(" enregistrée à l'adresse EEPROM : ");
        Serial.println(calVal_eepromAdress);
        resume = true;

      }
    else if (digitalRead(Bouton_2) == LOW) {
       Serial.println("Valeur non enregistrée dans l'EEPROM");
       resume = true;
    }
    
  }

  Serial.println("Fin de la calibration");
  Serial.println("***");
}