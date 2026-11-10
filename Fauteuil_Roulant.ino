#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_FXOS8700.h>
#include <Adafruit_FXAS21002C.h>
#include <math.h>
#include <esp_system.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <string.h>

// ---------- Configuration a adapter ----------

const char* WIFI_SSID = "Redmi Note 13 Pro 5G";
const char* WIFI_MDP = "shadowforsmash";

// Passer a true lorsque votre broker est disponible.
const bool MQTT_ACTIF = false;

// Exemple uniquement : remplacer par l'IP du Raspberry Pi.
const char* MQTT_SERVEUR = "10.20.109.15";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_UTILISATEUR = "fauteuil";
const char* MQTT_MDP = "A_REMPLACER";

const char* TOPIC_MESURES = "fauteuil/01/chute/mesures";
const char* TOPIC_ALERTES = "fauteuil/01/chute/alertes";
const char* TOPIC_STATUT = "fauteuil/01/chute/statut";



const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Fauteuil connecté</title>

<style>
* { box-sizing: border-box; }

body {
  margin: 0;
  font-family: Arial, sans-serif;
  background: #101827;
  color: #f8fafc;
}

header {
  background: #1e293b;
  padding: 22px;
  text-align: center;
}

header h1 { margin: 0 0 8px; }
header p { color: #94a3b8; margin: 0; }

main {
  max-width: 900px;
  margin: auto;
  padding: 18px;
}

.panel {
  background: #1e293b;
  border-radius: 16px;
  padding: 22px;
  margin-bottom: 18px;
  text-align: center;
}

h2 {
  font-size: 19px;
  margin-top: 0;
}

.value {
  font-size: 38px;
  font-weight: bold;
}

.gauge {
  width: 190px;
  height: 190px;
  border-radius: 50%;
  margin: 22px auto;
  display: grid;
  place-items: center;
  background: conic-gradient(
    #22c55e 0deg,
    #334155 0deg
  );
}

.gauge-inner {
  width: 148px;
  height: 148px;
  border-radius: 50%;
  background: #1e293b;
  display: grid;
  place-items: center;
}

#chair {
  width: 100%;
  max-width: 420px;
  height: 260px;
}

#movingChair {
  transition: transform 0.25s ease-out;
}

#status {
  font-size: 23px;
  font-weight: bold;
  color: #22c55e;
}

.info {
  color: #94a3b8;
  font-size: 13px;
}

.stats {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

.stat {
  background: #334155;
  border-radius: 12px;
  padding: 15px;
}

.stat strong {
  display: block;
  font-size: 23px;
  margin-top: 8px;
}

#connection {
  font-size: 13px;
  color: #22c55e;
}

.btn-reset {
  background: #dc2626;
  color: white;
  border: none;
  border-radius: 10px;
  padding: 12px 20px;
  margin-top: 18px;
  font-size: 15px;
  font-weight: bold;
  cursor: pointer;
}

.btn-reset:hover {
  background: #b91c1c;
}

</style>
</head>

<body>

<header>
  <h1>Fauteuil roulant connecté</h1>
  <p>Surveillance et détection de chute</p>
  <p id="connection">Connexion en cours...</p>
</header>

<main>

  <section class="panel">
    <h2>Inclinaison actuelle</h2>

    <div class="gauge" id="gauge">
      <div class="gauge-inner">
        <div>
          <div class="value" id="angle">0°</div>
          <small>Inclinaison</small>
        </div>
      </div>
    </div>

    <p class="info">Seuil de détection : 45° pendant 2 secondes</p>
  </section>

  <section class="panel">
    <h2>Visualisation du fauteuil</h2>

    <svg id="chair" viewBox="0 0 300 260">

      <!-- Sol -->
      <path d="M10 225 H290"
        stroke="#64748b" stroke-width="2"/>

      <!-- Ensemble mobile -->
      <g id="movingChair">

        <!-- Grande roue -->
        <circle cx="130" cy="180" r="45"
          fill="none" stroke="#94a3b8"
          stroke-width="9"/>

        <circle cx="130" cy="180" r="6"
          fill="#94a3b8"/>

        <!-- Chassis -->
        <path d="M130 180 L160 105 L225 105"
          fill="none" stroke="#38bdf8"
          stroke-width="10" stroke-linecap="round"/>

        <!-- Dossier -->
        <path d="M160 105 L160 38"
          fill="none" stroke="#38bdf8"
          stroke-width="12" stroke-linecap="round"/>

        <!-- Assise -->
        <path d="M155 105 L225 105 L225 119 L155 119 Z"
          fill="#38bdf8"/>

        <!-- Repose-pieds -->
        <path d="M225 112 L250 180"
          stroke="#38bdf8" stroke-width="9"
          stroke-linecap="round"/>

        <!-- Petite roue -->
        <circle cx="250" cy="190" r="13"
          fill="none" stroke="#94a3b8"
          stroke-width="6"/>

        <!-- Accoudoir -->
        <path d="M170 80 L220 80"
          stroke="#38bdf8" stroke-width="7"
          stroke-linecap="round"/>

      </g>

    </svg>

    <p id="orientation" class="info">
      Fauteuil en position normale
    </p>

    <p class="info">Représentation dynamique de l'inclinaison</p>
  </section>

  <section class="panel">
    <h2>État du fauteuil</h2>
    <p id="status">NORMAL</p>
  </section>

  <section class="panel">
    <h2>Mesures des capteurs</h2>

    <div class="stats">
      <div class="stat">
        Accélération
        <strong id="acceleration">0 g</strong>
      </div>

      <div class="stat">
        Rotation gyro
        <strong id="rotation">0 °/s</strong>
      </div>
    </div>

    <div class="stat">
      Inclinaison latérale
      <strong id="angleLateral">0°</strong>
    </div>
  </section>

  <section class="panel">
    <h2>Historique des incidents</h2>

    <p class="info">
      Chutes critiques et fausses alertes enregistrées
    </p>

    <div id="historique">
      Chargement de l'historique...
    </div>

    <button class="btn-reset" onclick="reinitialiserHistorique()">
      🗑️ Réinitialiser l'historique
    </button>
  </section>

</main>

<script>

async function actualiserInterface() {

  try {

    const reponse = await fetch('/api/data');

    if (!reponse.ok) throw new Error("Serveur indisponible");

    const data = await reponse.json();

    const angle = Number(data.angle_deg);

    if (!Number.isFinite(angle)) {
      throw new Error("Angle invalide");
    }

    // Affichage de l'angle
    document.getElementById('angle').textContent =
      angle.toFixed(1) + '°';

    // Animation du fauteuil
    // Rotation autour du centre de la grande roue
    
    const angleLateral = Number(data.angle_lateral_deg);

    const lateralDominant =
      Math.abs(angleLateral) > Math.abs(angle);

    const angleVisuel = lateralDominant
      ? angleLateral
      : angle;

    document.getElementById('movingChair')
      .setAttribute(
        'transform',
        `rotate(${angleVisuel} 130 180)`
      );

    const orientation = document.getElementById('orientation');

    if (lateralDominant && Math.abs(angleLateral) >= 30) {
      orientation.textContent =
        '⚠ Inclinaison latérale détectée';
    } else if (Math.abs(angle) >= 30) {
      orientation.textContent =
        '⚠ Inclinaison avant / arrière détectée';
    } else {
      orientation.textContent =
        'Fauteuil en position normale';
    }

    // Prendre en compte les deux axes
    const inclinaisonMax = Math.max(
      Math.abs(angle),
      Math.abs(angleLateral)
    );
    
    // Mise à jour de la jauge
    const progression = Math.min(inclinaisonMax, 90);
    const degres = progression / 90 * 360;
    
    // Détermination de la couleur
    let couleur = '#22c55e';
    
    if (inclinaisonMax >= 30) {
      couleur = '#f59e0b';
    }
    
    if (
      data.etat === 'chute' ||
      data.etat === 'chute_laterale' ||
      data.etat === 'chute_avant_arriere'
    ) {
      couleur = '#ef4444';
    }

    document.getElementById('gauge').style.background =
      `conic-gradient(${couleur} 0deg ${degres}deg,
      #334155 ${degres}deg 360deg)`;

    // Etat
    const status = document.getElementById('status');

    const nomsEtats = {
      normal: 'NORMAL',
      avertissement: 'AVERTISSEMENT',
      chute: 'CHUTE DÉTECTÉE',
      chute_laterale: 'CHUTE LATÉRALE',
      chute_avant_arriere: 'CHUTE AVANT / ARRIÈRE'
    };

    status.textContent = nomsEtats[data.etat] || data.etat.toUpperCase();
    status.style.color = couleur;

    // Autres mesures
    document.getElementById('acceleration').textContent =
      Number(data.acceleration_g).toFixed(2) + ' g';

    document.getElementById('rotation').textContent =
      Number(data.rotation_dps).toFixed(1) + ' °/s';

    document.getElementById('angleLateral').textContent =
      Number(data.angle_lateral_deg).toFixed(1) + '°';

    document.getElementById('connection').textContent =
      'ESP32 connecté';

    document.getElementById('connection').style.color =
      '#22c55e';

  } catch (erreur) {

    document.getElementById('connection').textContent =
      'ESP32 déconnecté';

    document.getElementById('connection').style.color =
      '#ef4444';
  }
}

async function reinitialiserHistorique() {

  const confirmation = confirm(
    "Attention ! Voulez-vous vraiment supprimer tout l'historique des incidents ?"
  );

  if (!confirmation) {
    return;
  }

  try {

    const reponse = await fetch('/api/historique/reset', {
      method: 'POST'
    });

    if (!reponse.ok) {
      throw new Error("Erreur lors de la suppression");
    }

    alert("L'historique a été réinitialisé.");

    // Actualiser immédiatement le tableau
    await actualiserHistorique();

  } catch (erreur) {

    alert("Impossible de réinitialiser l'historique.");

  }
}

async function actualiserHistorique() {
  const zone = document.getElementById('historique');

  try {
    const reponse = await fetch('/api/historique');

    if (!reponse.ok) {
      throw new Error("Erreur historique");
    }

    const evenements = await reponse.json();

    // Conserver le dernier état de chaque incident
    const incidents = new Map();

    evenements.forEach(e => {
      incidents.set(e.incident_id, e);
    });

    const liste = Array.from(incidents.values()).reverse();

    zone.replaceChildren();

    if (liste.length === 0) {
      zone.textContent = "Aucun incident enregistré.";
      return;
    }

    const tableau = document.createElement('table');

    tableau.style.width = '100%';
    tableau.style.borderCollapse = 'collapse';

    const entete = document.createElement('tr');

    ['Incident', 'Résultat', 'Cause', 'Angle', 'Temps'].forEach(t => {
      const th = document.createElement('th');
      th.textContent = t;
      th.style.padding = '8px';
      entete.appendChild(th);
    });

    tableau.appendChild(entete);

    liste.forEach(e => {
      const ligne = document.createElement('tr');

      const resultat = e.niveau === 'avertissement'
        ? 'FAUSSE ALERTE'
        : 'CHUTE CRITIQUE';

      const cellules = [
        e.incident_id,
        resultat,
        e.cause,
        Number(e.angle_deg).toFixed(1) + '°',
        (Number(e.uptime_ms) / 1000).toFixed(0) + ' s'
      ];

      cellules.forEach((valeur, index) => {
        const td = document.createElement('td');
        td.textContent = valeur;
        td.style.padding = '9px 5px';
        td.style.borderTop = '1px solid #475569';

        if (index === 1) {
          td.style.color = e.niveau === 'avertissement'
            ? '#f59e0b'
            : '#ef4444';

          td.style.fontWeight = 'bold';
        }

        ligne.appendChild(td);
      });

      tableau.appendChild(ligne);
    });

    zone.appendChild(tableau);

  } catch (erreur) {
    zone.textContent = "Impossible de charger l'historique.";
  }
}

actualiserHistorique();
setInterval(actualiserHistorique, 5000);

setInterval(actualiserInterface, 250);
actualiserInterface();

</script>

</body>
</html>
)rawliteral";


// ---------- Serveur web ----------

WebServer server(80);

bool historiqueDisponible = false;

// Donnees partagees avec l'interface
float angleActuel = 0.0f;
float angleLateralActuel = 0.0f;
float accelerationGActuelle = 0.0f;
float rotationDpsActuelle = 0.0f;

// ---------- Materiel et seuils de test ----------

const int BUZZER = 25;
const int LED_ROUGE = 26;

const float SEUIL_ANGLE = 45.0f;
const float ANGLE_REDRESSE = 35.0f;
const float SEUIL_CHOC_G = 3.0f;
const float SEUIL_ROTATION_REPOS = 8.0f;

const unsigned long DUREE_BASCULEMENT = 2000;
const unsigned long DUREE_IMMOBILITE = 2000;
const unsigned long FENETRE_CHOC = 5000;
const unsigned long DELAI_REDRESSEMENT = 10000;

Adafruit_FXOS8700 accelerometre;
Adafruit_FXAS21002C gyroscope;

// ---------- Communication entre les deux taches ----------

struct Message {
  char texte[320];
};

QueueHandle_t fileAlertes;
QueueHandle_t derniereMesure;

char session[12];
unsigned long numeroIncident = 0;

// ---------- Etat de detection ----------

bool temporisationAngle = false;
bool chocEnAttente = false;
bool immobiliteEnCours = false;

bool alerteActive = false;
bool inclinaisonObservee = false;
bool retourEnCours = false;

unsigned long debutAngle = 0;
unsigned long instantChoc = 0;
unsigned long debutImmobilite = 0;
unsigned long instantAlerte = 0;
unsigned long debutRetour = 0;
unsigned long dernierEnvoi = 0;
unsigned long dernierAffichage = 0;

const char* causeAlerte = "aucune";

// ---------- Evenements et signalisation ----------

void evenement(const char* etat, const char* niveau,
               const char* cause, float angle) {
  Message message;

  snprintf(
    message.texte, sizeof(message.texte),
    "{\"module\":\"chute\","
    "\"incident\":\"%s-%lu\","
    "\"uptime_ms\":%lu,"
    "\"etat\":\"%s\","
    "\"niveau\":\"%s\","
    "\"cause\":\"%s\","
    "\"angle_deg\":%.1f}",
    session, numeroIncident, millis(),
    etat, niveau, cause, angle
  );

  Serial.println(message.texte);

  if (MQTT_ACTIF &&
      xQueueSend(fileAlertes, &message, 0) != pdTRUE) {
    Serial.println("File MQTT pleine : evenement non memorise.");
  }
}

void signalLocal(bool actif) {
  digitalWrite(LED_ROUGE, actif ? HIGH : LOW);

  if (actif) {
    tone(BUZZER, 2000);
  } else {
    noTone(BUZZER);
  }
}

void enregistrerHistorique(
  const char* typeEvenement,
  const char* niveau,
  const char* cause,
  float angle,
  unsigned long maintenant
) {
  if (!historiqueDisponible) return;

  File fichier = LittleFS.open("/historique.jsonl", FILE_APPEND);

  if (!fichier) {
    Serial.println("Erreur ouverture historique.");
    return;
  }

  fichier.printf(
    "{\"incident_id\":\"%s-%lu\","
    "\"evenement\":\"%s\","
    "\"niveau\":\"%s\","
    "\"cause\":\"%s\","
    "\"angle_deg\":%.1f,"
    "\"uptime_ms\":%lu}\n",
    session,
    numeroIncident,
    typeEvenement,
    niveau,
    cause,
    angle,
    maintenant
  );

  fichier.close();

  Serial.println("Historique mis a jour.");
}

void declencher(const char* cause, float angle,
                unsigned long maintenant) {
  if (alerteActive) return;

  numeroIncident++;
  alerteActive = true;
  instantAlerte = maintenant;
  causeAlerte = cause;

  inclinaisonObservee = fabsf(angle) > SEUIL_ANGLE;
  retourEnCours = false;
  chocEnAttente = false;
  immobiliteEnCours = false;

  signalLocal(true);

  enregistrerHistorique(
    "declenchee",
    "critique",
    causeAlerte,
    angle,
    maintenant
  );

  evenement("declenchee", "critique", causeAlerte, angle);
}


void configurerServeurWeb() {

  // Page principale
  server.on("/", HTTP_GET, []() {

    server.send_P(
      200,
      "text/html; charset=utf-8",
      PAGE_HTML
    );

  });

  // Donnees des capteurs
  
server.on("/api/data", HTTP_GET, []() {

  const char* etat = "normal";

  if (alerteActive) {
    if (strcmp(causeAlerte, "basculement_lateral") == 0) {
      etat = "chute_laterale";
    } else if (
      strcmp(causeAlerte, "basculement_avant_arriere") == 0
    ) {
      etat = "chute_avant_arriere";
    } else {
      etat = "chute";
    }
  }
  else if (
    max(fabsf(angleActuel), fabsf(angleLateralActuel)) >= 30.0f
  ) {
    etat = "avertissement";
  }

  char json[250];

  snprintf(
    json,
    sizeof(json),
    "{"
    "\"angle_deg\":%.2f,"
    "\"angle_lateral_deg\":%.2f,"
    "\"acceleration_g\":%.2f,"
    "\"rotation_dps\":%.2f,"
    "\"alerte_active\":%s,"
    "\"etat\":\"%s\""
    "}",
    angleActuel,
    angleLateralActuel,
    accelerationGActuelle,
    rotationDpsActuelle,
    alerteActive ? "true" : "false",
    etat
  );

  server.send(200, "application/json", json);

});


  server.on("/api/historique", HTTP_GET, []() {

    if (!historiqueDisponible) {
      server.send(
        500,
        "application/json",
        "{\"erreur\":\"Historique indisponible\"}"
      );
      return;
    }

    File fichier = LittleFS.open("/historique.jsonl", FILE_READ);

    if (!fichier) {
      server.send(200, "application/json", "[]");
      return;
    }

    String json = "[";
    bool premier = true;

    while (fichier.available()) {
      String ligne = fichier.readStringUntil('\n');
      ligne.trim();

      if (ligne.length() > 0) {
        if (!premier) json += ",";
        json += ligne;
        premier = false;
      }
    }

    fichier.close();
    json += "]";

    server.send(200, "application/json", json);
  });

  // ---------- Reinitialisation de l'historique ----------

  server.on("/api/historique/reset", HTTP_POST, []() {
  
    if (!historiqueDisponible) {
      server.send(
        500,
        "application/json",
        "{\"erreur\":\"Historique indisponible\"}"
      );
      return;
    }
  
    // Supprimer le fichier historique
    if (LittleFS.exists("/historique.jsonl")) {
    
      if (!LittleFS.remove("/historique.jsonl")) {
      
        server.send(
          500,
          "application/json",
          "{\"erreur\":\"Suppression impossible\"}"
        );
  
        return;
      }
    }
  
    Serial.println("Historique reinitialise depuis le site.");
  
    server.send(
      200,
      "application/json",
      "{\"succes\":true,\"message\":\"Historique reinitialise\"}"
    );
  
  });

  server.begin();

  Serial.println("Serveur web demarre.");
}


// ---------- Reseau dans une tache separee ----------
// Les tentatives MQTT ne bloquent pas la lecture des capteurs.

void tacheReseau(void* parametre) {
  WiFiClient transport;
  PubSubClient mqtt(transport);

  mqtt.setServer(MQTT_SERVEUR, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(15);
  mqtt.setSocketTimeout(1);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_MDP);

  String identifiant = "fauteuil01-chute-" + String(session);

  unsigned long dernierEssai = millis() - 5000;
  unsigned long dernierStatut = 0;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long maintenant = millis();

      if (!mqtt.connected() &&
          maintenant - dernierEssai >= 5000) {
        dernierEssai = maintenant;

        bool connecte;

        if (strlen(MQTT_UTILISATEUR) > 0) {
          connecte = mqtt.connect(
            identifiant.c_str(),
            MQTT_UTILISATEUR, MQTT_MDP,
            TOPIC_STATUT, 1, true, "hors_ligne"
          );
        } else {
          connecte = mqtt.connect(
            identifiant.c_str(),
            TOPIC_STATUT, 1, true, "hors_ligne"
          );
        }

        if (connecte) {
          Serial.println("MQTT connecte.");
          mqtt.publish(TOPIC_STATUT, "en_ligne", true);
          dernierStatut = millis();
        } else {
          Serial.print("Echec MQTT, code : ");
          Serial.println(mqtt.state());
        }
      }

      if (mqtt.connected()) {
        mqtt.loop();

        Message message;

        // Les alertes passent avant les mesures.
        if (xQueuePeek(fileAlertes, &message, 0) == pdTRUE) {
          if (mqtt.publish(TOPIC_ALERTES, message.texte)) {
            xQueueReceive(fileAlertes, &message, 0);
          }
        } else if (
          xQueueReceive(derniereMesure, &message, 0) == pdTRUE
        ) {
          mqtt.publish(TOPIC_MESURES, message.texte);
        }

        if (millis() - dernierStatut >= 15000) {
          if (mqtt.publish(TOPIC_STATUT, "en_ligne", true)) {
            dernierStatut = millis();
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------- Initialisation ----------

void setup() {
  Serial.begin(115200);

    Serial.println();
  Serial.println("===== DIAGNOSTIC MEMOIRE =====");

  const esp_partition_t* partition =
    esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
      "spiffs"
    );

  if (partition != nullptr) {
    Serial.println("Partition SPIFFS trouvee !");
    Serial.print("Taille : ");
    Serial.print(partition->size);
    Serial.println(" octets");

    if (LittleFS.begin(true)) {
      historiqueDisponible = true;
      Serial.println("LittleFS initialise et pret !");
    } else {
      historiqueDisponible = false;
      Serial.println("Echec initialisation LittleFS.");
    }

  } else {
    historiqueDisponible = false;
    Serial.println("ERREUR : aucune partition SPIFFS trouvee !");
  }

  Serial.println("===== FIN DIAGNOSTIC =====");

  pinMode(LED_ROUGE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  signalLocal(false);

  delay(1000);
  Wire.begin(21, 22);

  if (!accelerometre.begin() || !gyroscope.begin()) {
    Serial.println("ERREUR : un capteur n'est pas detecte.");

    while (true) {
      digitalWrite(LED_ROUGE, HIGH);
      delay(200);
      digitalWrite(LED_ROUGE, LOW);
      delay(200);
    }
  }

  accelerometre.setAccelRange(ACCEL_RANGE_8G);

  // ---------- Demarrage du Wi-Fi ----------

  // ---------- Connexion au reseau Wi-Fi ----------

WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
WiFi.setSleep(false);

Serial.println();
Serial.println("Connexion au Wi-Fi...");
Serial.print("Reseau demande : ");
Serial.println(WIFI_SSID);

WiFi.begin(WIFI_SSID, WIFI_MDP);

// Attendre au maximum 20 secondes
unsigned long debutConnexion = millis();

while (WiFi.status() != WL_CONNECTED &&
       millis() - debutConnexion < 20000) {

  Serial.print(".");
  delay(500);
}

Serial.println();

if (WiFi.status() == WL_CONNECTED) {
  Serial.println("WIFI CONNECTE !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  Serial.print("Puissance du signal : ");
  Serial.println(WiFi.RSSI());
} else {
  Serial.println("ECHEC DE CONNEXION WIFI !");
  Serial.print("Code erreur Wi-Fi : ");
  Serial.println(WiFi.status());
}

// Demarrage du serveur web
configurerServeurWeb();


  snprintf(session, sizeof(session), "%08lx",
           (unsigned long)esp_random());

  if (MQTT_ACTIF) {
    fileAlertes = xQueueCreate(16, sizeof(Message));
    derniereMesure = xQueueCreate(1, sizeof(Message));

    if (fileAlertes == nullptr || derniereMesure == nullptr ||
        xTaskCreate(
          tacheReseau, "reseau", 6144, nullptr, 1, nullptr
        ) != pdPASS) {
      Serial.println("ERREUR : demarrage reseau impossible.");
      while (true) delay(100);
    }
  }

  Serial.println("Detection prete.");
  Serial.println("Envoyer r pour rearmer manuellement une alerte.");
}

// ---------- Surveillance ----------

void loop() {
  // Gestion du serveur web
  server.handleClient();

  sensors_event_t acceleration, magnetisme, rotation;

  if (!accelerometre.getEvent(&acceleration, &magnetisme) ||
      !gyroscope.getEvent(&rotation)) {
    temporisationAngle = false;
    chocEnAttente = false;
    immobiliteEnCours = false;
    retourEnCours = false;

    Serial.println("ERREUR de lecture des capteurs.");
    delay(100);
    return;
  }

  float ax = acceleration.acceleration.x;
  float ay = acceleration.acceleration.y;
  float az = acceleration.acceleration.z;

  float norme = sqrt(ax * ax + ay * ay + az * az);

  if (norme < 0.1f) {
    temporisationAngle = false;
    immobiliteEnCours = false;
    retourEnCours = false;
    delay(10);
    return;
  }

  float accelerationG = norme / 9.80665f;

  // Inclinaison selon l'axe Y, orienté vers l'avant
  // Inclinaison avant / arrière
float angle = atan2(
  ay,
  sqrt(ax * ax + az * az)
) * 180.0f / PI;

// Inclinaison latérale gauche / droite
float angleLateral = atan2(
  ax,
  sqrt(ay * ay + az * az)
) * 180.0f / PI;

// Plus grande inclinaison détectée
float inclinaison = max(
  fabsf(angle),
  fabsf(angleLateral)
);

  float gx = rotation.gyro.x;
  float gy = rotation.gyro.y;
  float gz = rotation.gyro.z;

  float rotationDps =
    sqrt(gx * gx + gy * gy + gz * gz) * 180.0f / PI;

  unsigned long maintenant = millis();

  // Mise a jour des donnees accessibles par le serveur
  angleActuel = angle;
  angleLateralActuel = angleLateral;
  accelerationGActuelle = accelerationG;
  rotationDpsActuelle = rotationDps;

  bool immobile =
    accelerationG >= 0.9f &&
    accelerationG <= 1.1f &&
    rotationDps < SEUIL_ROTATION_REPOS;

  if (!alerteActive) {
    // Methode 1 : basculement prolonge.
    if (inclinaison > SEUIL_ANGLE) {
      if (!temporisationAngle) {
        temporisationAngle = true;
        debutAngle = maintenant;
      }

      if (maintenant - debutAngle > DUREE_BASCULEMENT) {

        if (fabsf(angleLateral) > fabsf(angle)) {
          declencher("basculement_lateral", angleLateral, maintenant);
        } else {
          declencher("basculement_avant_arriere", angle, maintenant);
        }
      }
    } else {
      temporisationAngle = false;
    }

    // Methode 2 : choc puis immobilite.
    if (!alerteActive) {
      if (accelerationG > SEUIL_CHOC_G) {
        if (!chocEnAttente) {
          chocEnAttente = true;
          instantChoc = maintenant;
          Serial.println("Choc : recherche d'immobilite.");
        }
        immobiliteEnCours = false;
      }

      if (chocEnAttente) {
        if (maintenant - instantChoc > FENETRE_CHOC) {
          chocEnAttente = false;
          immobiliteEnCours = false;
          Serial.println("Choc non confirme.");
        } else if (immobile) {
          if (!immobiliteEnCours) {
            immobiliteEnCours = true;
            debutImmobilite = maintenant;
          }

          if (maintenant - debutImmobilite >= DUREE_IMMOBILITE) {
            declencher("choc_immobilite", angle, maintenant);
          }
        } else {
          immobiliteEnCours = false;
        }
      }
    }
  }

  // Une alerte de choc peut survenir alors que le fauteuil
  // est deja horizontal : ne pas l'annuler automatiquement.
  if (alerteActive) {
    if (inclinaison > SEUIL_ANGLE) {
  inclinaisonObservee = true;
  }

    if (inclinaisonObservee &&
      inclinaison < ANGLE_REDRESSE && immobile) {
      if (!retourEnCours) {
        retourEnCours = true;
        debutRetour = maintenant;
      }

      // Confirmation pendant 300 ms pour eviter les oscillations.
      if (maintenant - debutRetour >= 300) {
        bool rapide =
          maintenant - instantAlerte <= DELAI_REDRESSEMENT;

        enregistrerHistorique(
          rapide ? "fausse_alerte" : "redressement_tardif",
          rapide ? "avertissement" : "critique",
          causeAlerte,
          angle,
          maintenant
        );

        signalLocal(false);
        alerteActive = false;
        temporisationAngle = false;
        retourEnCours = false;
      }
    } else {
      retourEnCours = false;
    }
  }

  // Rearmement manuel depuis le Moniteur serie.
  if (Serial.available()) {
    char commande = Serial.read();

    if (commande == 'r' && alerteActive) {
      evenement("rearmement_manuel", "critique",
                causeAlerte, angle);

      enregistrerHistorique(
        "rearmement_manuel",
        "critique",
        causeAlerte,
        angle,
        millis()
      );  

      signalLocal(false);
      alerteActive = false;
      temporisationAngle = false;
      chocEnAttente = false;
      immobiliteEnCours = false;
      retourEnCours = false;
    }
  }

  if (maintenant - dernierEnvoi >= 5000) {
    dernierEnvoi = maintenant;

    Message message;

    snprintf(
      message.texte, sizeof(message.texte),
      "{\"module\":\"chute\","
      "\"session\":\"%s\","
      "\"uptime_ms\":%lu,"
      "\"angle_deg\":%.1f,"
      "\"acceleration_g\":%.2f,"
      "\"rotation_dps\":%.1f,"
      "\"alerte_active\":%s}",
      session, maintenant, angle,
      accelerationG, rotationDps,
      alerteActive ? "true" : "false"
    );

    Serial.println(message.texte);

    if (MQTT_ACTIF) {
      xQueueOverwrite(derniereMesure, &message);
    }
  }

  if (maintenant - dernierAffichage >= 500) {
    dernierAffichage = maintenant;

    Serial.print("Angle : ");
    Serial.print(angle, 1);
    Serial.print(" deg | Accel : ");
    Serial.print(accelerationG, 2);
    Serial.print(" g | Rotation : ");
    Serial.print(rotationDps, 1);
    Serial.println(" deg/s");
  }

  delay(10);

  static unsigned long dernierEssaiWifi = 0;

  if (WiFi.status() != WL_CONNECTED &&
    millis() - dernierEssaiWifi >= 10000) {

  dernierEssaiWifi = millis();

  WiFi.reconnect();

  }

  if (WiFi.status() == WL_CONNECTED) {
    static bool ipAffichee = false;

    if (!ipAffichee) {
      Serial.print("Adresse IP du fauteuil : ");
      Serial.println(WiFi.localIP());
      ipAffichee = true;
    }
  }
}