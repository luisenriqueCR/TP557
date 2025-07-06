#include <tflm_esp32.h>
#include <eloquent_tinyml.h>
#include "crop_yield_model.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define NUM_INPUTS 4
#define NUM_OUTPUTS 1
#define ARENA_SIZE 2000
#define TF_NUM_OPS 4

const char* ssid = "ARLE";
const char* password = "aaeeaarr";

Eloquent::TF::Sequential<TF_NUM_OPS, ARENA_SIZE> tf;

float input[NUM_INPUTS];
float input_features[NUM_INPUTS];
float mean[NUM_INPUTS]  = {1.5, 23.81041915, 74.25899596, 6.60175931};
float scale[NUM_INPUTS] = {1.11803399, 8.90597692, 6.74026108, 0.81956434};

WebServer server(80);

unsigned long previousMillis = 0;
const unsigned long interval = 30000;

// Estructura para almacenar predicciones
struct Prediction {
  float temperature;
  float humidity;
  float ph;
  int cropType;
  float result;
  unsigned long timestamp;
};

// Estructura para almacenar reglas de validación por cultivo
struct CropValidationRange {
  float minTemp;
  float maxTemp;
  float minHumidity;
};

// Rangos para Maiz/Milho
const CropValidationRange maizeRanges[] = {
  {6.0, 9.9, 82.0},    
  {10.0, 11.9, 80.0},   
  {12.0, 19.9, 79.0},   
  {20.0, 32.0, 75.0}      
};
const int maizeRangesCount = 4;

// Rangos para Papa/Batata
const CropValidationRange potatoRanges[] = {
  {5.0, 9.9, 86.0},    
  {10.0, 16.9, 80.0},   
  {17.0, 23.9, 76.0},   
  {24.0, 32.0, 73.0}    
};
const int potatoRangesCount = 4;

// Rangos para Arroz
const CropValidationRange riceRanges[] = {
  {15.0, 15.9, 80.0},   
  {16.0, 16.9, 78.0},   
  {17.0, 18.9, 77.0},   
  {19.0, 32.0, 73.0}    
};
const int riceRangesCount = 4;

// Rangos para Trigo
const CropValidationRange wheatRanges[] = {
  {9.0, 10.9, 86.0},    
  {11.0, 13.9, 79.0},   
  {14.0, 18.9, 77.0},   
  {19.0, 32.0, 75.0}    
};
const int wheatRangesCount = 4;

Prediction predictions[10]; 
int predictionCount = 0;

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado a WiFi");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

String getMainHTML() {
  String html = "";
  
  // HTML Header y CSS
  html += "<!DOCTYPE html>";
  html += "<html lang=\"es\">";
  html += "<head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>CropAssist - Monitoramiento de Cultivos</title>";
  html += "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\">";
  html += "<style>";
  
  // CSS Moderno
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Inter', sans-serif; background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%); min-height: 100vh; color: #333; }";
  html += ".container { max-width: 1400px; margin: 0 auto; padding: 20px; }";
  
  // Header estilo moderno
  html += ".header { background: #2E6F56; color: white; padding: 24px; border-radius: 16px; margin-bottom: 24px; box-shadow: 0 8px 32px rgba(46, 111, 86, 0.2); }";
  html += ".header h1 { font-size: 28px; font-weight: 600; margin-bottom: 8px; }";
  html += ".header p { opacity: 0.9; font-size: 16px; }";
  
  // Grid principal modificado para dos columnas
  html += ".main-content { display: grid; grid-template-columns: 1fr 1fr; gap: 24px; margin-bottom: 24px; }";
  html += "@media (max-width: 1024px) { .main-content { grid-template-columns: 1fr; gap: 20px; } }";
  
  // Cards con diseño moderno
  html += ".card { background: white; border-radius: 16px; padding: 24px; box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1); transition: all 0.3s ease; height: fit-content; }";
  html += ".card:hover { transform: translateY(-4px); box-shadow: 0 8px 32px rgba(0, 0, 0, 0.15); }";
  html += ".card h3 { color: #2E6F56; margin-bottom: 20px; font-size: 20px; font-weight: 600; }";
  
  // Selección de cultivos estilo moderno
  html += ".crops-selection { display: grid; grid-template-columns: repeat(2, 1fr); gap: 16px; margin-bottom: 24px; }";
  html += ".crop-card { background: #f8f9fa; border: 2px solid transparent; border-radius: 12px; padding: 16px; text-align: center; cursor: pointer; transition: all 0.3s ease; }";
  html += ".crop-card:hover { background: #f0f8f4; border-color: #2E6F56; transform: translateY(-2px); }";
  html += ".crop-card.selected { background: #2E6F56; color: white; border-color: #2E6F56; }";
  html += ".crop-icon { width: 48px; height: 48px; background: #2E6F56; border-radius: 12px; display: flex; align-items: center; justify-content: center; margin: 0 auto 12px; font-size: 24px; transition: all 0.3s ease; }";
  html += ".crop-card.selected .crop-icon { background: white; color: #2E6F56; }";
  html += ".crop-name { font-weight: 500; font-size: 14px; }";
  
  // Formulario moderno
  html += ".form-group { margin-bottom: 20px; }";
  html += ".form-group label { display: block; margin-bottom: 8px; font-weight: 500; color: #333; font-size: 14px; }";
  html += ".form-group input { width: 100%; padding: 12px 16px; border: 2px solid #e0e0e0; border-radius: 8px; font-size: 16px; transition: all 0.3s ease; background: #fafafa; }";
  html += ".form-group input:focus { outline: none; border-color: #2E6F56; background: white; box-shadow: 0 0 0 3px rgba(46, 111, 86, 0.1); }";
  
  // Botón moderno
  html += ".btn { background: linear-gradient(135deg, #2E6F56, #4a8f73); color: white; border: none; padding: 14px 24px; border-radius: 8px; font-size: 16px; font-weight: 500; cursor: pointer; transition: all 0.3s ease; width: 100%; position: relative; overflow: hidden; }";
  html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 8px 24px rgba(46, 111, 86, 0.3); }";
  html += ".btn:disabled { background: #ccc; cursor: not-allowed; transform: none; box-shadow: none; }";
  html += ".btn:active { transform: translateY(0); }";
  
  // Resultado con animación
  html += ".result-display { background: linear-gradient(135deg, #2E6F56, #4a8f73); color: white; padding: 24px; border-radius: 16px; margin-top: 20px; text-align: center; animation: slideUp 0.5s ease; position: relative; overflow: hidden; }";
  html += ".result-display::before { content: ''; position: absolute; top: -50%; left: -50%; width: 200%; height: 200%; background: linear-gradient(45deg, transparent, rgba(255,255,255,0.1), transparent); transform: rotate(45deg); animation: shimmer 2s infinite; }";
  html += "@keyframes shimmer { 0% { transform: translateX(-100%) translateY(-100%) rotate(45deg); } 100% { transform: translateX(100%) translateY(100%) rotate(45deg); } }";
  html += "@keyframes slideUp { from { opacity: 0; transform: translateY(20px); } to { opacity: 1; transform: translateY(0); } }";
  html += ".result-display h4 { font-size: 18px; margin-bottom: 8px; font-weight: 500; }";
  html += ".result-display .value { font-size: 32px; font-weight: 700; margin-bottom: 8px; }";
  html += ".result-display .unit { font-size: 14px; opacity: 0.9; }";
  
  // Historial 
  html += ".history-section { background: white; border-radius: 16px; padding: 24px; box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1); height: fit-content; }";
  html += ".history-item { padding: 16px; border-bottom: 1px solid #f0f0f0; border-radius: 8px; transition: all 0.3s ease; margin-bottom: 12px; }";
  html += ".history-item:hover { background: #f8f9fa; }";
  html += ".history-item:last-child { border-bottom: none; margin-bottom: 0; }";
  html += ".history-date { font-weight: 500; color: #2E6F56; font-size: 13px; margin-bottom: 8px; }";
  // AGREGA estas nuevas líneas:
  html += ".history-data { display: table; width: 100%; table-layout: fixed; font-size: 13px; color: #666; line-height: 1.4; }";
  html += ".history-inputs { display: table-cell; width: 70%; padding-right: 16px; vertical-align: middle; }";
  html += ".history-result { display: table-cell; width: 30%; font-weight: 600; color: #2E6F56; font-size: 16px; text-align: right; vertical-align: middle; }";
  html += ".history-crop { display: inline-block; background: #f0f8f4; color: #2E6F56; padding: 4px 8px; border-radius: 6px; font-size: 12px; font-weight: 500; margin-bottom: 8px; }";
  
  // Loading spinner mejorado
  html += ".loading { display: inline-block; width: 20px; height: 20px; border: 2px solid rgba(255,255,255,0.3); border-top: 2px solid white; border-radius: 50%; animation: spin 1s linear infinite; margin-left: 10px; }";
  html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
  
  // Notificación
  html += ".notification { position: fixed; top: 20px; right: 20px; background: linear-gradient(135deg, #2E6F56, #4a8f73); color: white; padding: 16px 24px; border-radius: 12px; box-shadow: 0 8px 32px rgba(46, 111, 86, 0.3); transform: translateX(100%); transition: transform 0.3s ease; z-index: 1001; backdrop-filter: blur(10px); }";
  html += ".notification.show { transform: translateX(0); }";
  
  // Responsive mejorado
  html += "@media (max-width: 1024px) {";
  html += ".main-content { grid-template-columns: 1fr; }";
  html += "}";
  html += "@media (max-width: 768px) {";
  html += ".crops-selection { grid-template-columns: repeat(2, 1fr); gap: 12px; }";
  html += ".crop-card { padding: 12px; }";
  html += ".crop-icon { width: 40px; height: 40px; margin-bottom: 8px; font-size: 20px; }";
  html += ".result-display .value { font-size: 28px; }";
  html += ".container { padding: 15px; }";
  html += ".card { padding: 20px; }";
  html += ".history-data { display: block; }";
  html += ".history-inputs { display: block; width: 100%; padding-right: 0; margin-bottom: 8px; }";
  html += ".history-result { display: block; width: 100%; text-align: left; }";
  html += "}";
  
  html += "</style>";
  html += "</head>";
  html += "<body>";

  // HTML Body
  html += "<div class=\"container\">";
  html += "<div class=\"header\">";
  html += "<h1>🌱 CropAssist</h1>";
  html += "<p>Monitoramento Inteligente de Cultivos</p>";
  html += "</div>";
  
  html += "<div class=\"main-content\">";
  
  // Columna izquierda - Formulario de predicción
  html += "<div class=\"card\">";
  html += "<h3>📊 Nova previsão</h3>";
  
  // Selección de cultivos mejorada
  html += "<div class=\"crops-selection\">";
  html += "<div class=\"crop-card\" onclick=\"selectCrop(0)\" id=\"crop0\">";
  html += "<div class=\"crop-icon\">🌽</div>";
  html += "<div class=\"crop-name\">Milho</div>";
  html += "</div>";
  html += "<div class=\"crop-card\" onclick=\"selectCrop(1)\" id=\"crop1\">";
  html += "<div class=\"crop-icon\">🥔</div>";
  html += "<div class=\"crop-name\">Batata</div>";
  html += "</div>";
  html += "<div class=\"crop-card\" onclick=\"selectCrop(2)\" id=\"crop2\">";
  html += "<div class=\"crop-icon\">🌾</div>";
  html += "<div class=\"crop-name\">Arroz</div>";
  html += "</div>";
  html += "<div class=\"crop-card\" onclick=\"selectCrop(3)\" id=\"crop3\">";
  html += "<div class=\"crop-icon\">🌾</div>";
  html += "<div class=\"crop-name\">Trigo</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<form id=\"predictionForm\" onsubmit=\"submitPrediction(event)\">";
  html += "<input type=\"hidden\" id=\"cropType\" value=\"0\">";
  html += "<div class=\"form-group\">";
  html += "<label for=\"temperature\">🌡️ Temperatura (°C)</label>";
  html += "<input type=\"number\" id=\"temperature\" step=\"0.1\" placeholder=\"Ej: 27.5\" required>";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"humidity\">💧 Umidade Relativa do Ar (%)</label>";
  html += "<input type=\"number\" id=\"humidity\" step=\"0.1\" min=\"0\" max=\"100\" placeholder=\"Ej: 79.2\" required>";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"ph\">⚗️ pH do Solo</label>";
  html += "<input type=\"number\" id=\"ph\" step=\"0.1\" min=\"0\" max=\"14\" placeholder=\"Ej: 5.8\" required>";
  html += "</div>";
  html += "<button type=\"submit\" class=\"btn\" id=\"predictBtn\">";
  html += "<span id=\"btnText\">🎯 Prever Rendimiento</span>";
  html += "<span id=\"btnLoading\" class=\"loading\" style=\"display: none;\"></span>";
  html += "</button>";
  html += "</form>";
  html += "<div id=\"resultDisplay\" style=\"display: none;\"></div>";
  html += "</div>";
  
  // Columna derecha - Historial
  html += "<div class=\"history-section\">";
  html += "<h3 style=\"color: #2E6F56; margin-bottom: 20px; display: flex; align-items: center; gap: 8px;\">📋 Histórico de Observações e Previsões</h3>";
  html += "<div id=\"historyList\">";
  html += "<p style=\"text-align: center; color: #666; padding: 40px 20px; font-size: 14px;\">";
  html += "🌱 Ainda não há previsões.<br>Faça sua primeira previsão para começar.";
  html += "</p>";
  html += "</div>";
  html += "</div>";
  
  html += "</div>"; // Cierre main-content
  html += "</div>"; // Cierre container

  // Notification
  html += "<div class=\"notification\" id=\"notification\"></div>";

  // JavaScript mejorado
  html += "<script>";
  html += "var selectedCropType = 0;";
  html += "var predictions = [];";
  
  // Función para mostrar notificaciones
  html += "function showNotification(message, type = 'success') {";
  html += "var notification = document.getElementById('notification');";
  html += "notification.textContent = message;";
  html += "notification.className = 'notification show';";
  html += "setTimeout(function() { notification.classList.remove('show'); }, 4000);";
  html += "}";
  
  html += "function selectCrop(cropType) {";
  html += "for (var i = 0; i < 4; i++) {";
  html += "document.getElementById('crop' + i).classList.remove('selected');";
  html += "}";
  html += "document.getElementById('crop' + cropType).classList.add('selected');";
  html += "document.getElementById('cropType').value = cropType;";
  html += "selectedCropType = cropType;";
  html += "}";
  
  html += "function submitPrediction(event) {";
  html += "event.preventDefault();";
  html += "var btn = document.getElementById('predictBtn');";
  html += "var btnText = document.getElementById('btnText');";
  html += "var btnLoading = document.getElementById('btnLoading');";
  html += "btn.disabled = true;";
  html += "btnText.style.display = 'none';";
  html += "btnLoading.style.display = 'inline-block';";
  html += "var formData = new FormData();";
  html += "formData.append('c', document.getElementById('cropType').value);";
  html += "formData.append('t', document.getElementById('temperature').value);";
  html += "formData.append('h', document.getElementById('humidity').value);";
  html += "formData.append('p', document.getElementById('ph').value);";
  html += "fetch('/predict?' + new URLSearchParams(formData))";
  html += ".then(function(response) {";
  html += "if (!response.ok) {";
  html += "return response.json().then(function(errorData) {";
  html += "throw new Error(JSON.stringify(errorData));";
  html += "});";
  html += "}";
  html += "return response.text();";
  html += "})";
  html += ".then(function(result) {";
  html += "var predictionValue = parseFloat(result) || 0;";
  html += "var newPrediction = {";
  html += "cropType: parseInt(document.getElementById('cropType').value),";
  html += "temperature: parseFloat(document.getElementById('temperature').value),";
  html += "humidity: parseFloat(document.getElementById('humidity').value),";
  html += "ph: parseFloat(document.getElementById('ph').value),";
  html += "result: predictionValue,";
  html += "timestamp: new Date()";
  html += "};";
  html += "predictions.push(newPrediction);";
  html += "showResult(newPrediction);";
  html += "updateHistory();";
  html += "document.getElementById('predictionForm').reset();";
  html += "selectCrop(0);";
  html += "showNotification('✅ Previsão feita com sucesso: ' + predictionValue.toFixed(2) + ' ton/ha');";
  html += "})";
  html += ".catch(function(error) {";
  html += "console.error('Erro:', error);";
  html += "try {";
  html += "var errorData = JSON.parse(error.message);";
  html += "if (errorData.error) {";
  html += "showNotification(errorData.message + ': ' + errorData.details, 'error');";
  html += "} else {";
  html += "showNotification('❌ Erro ao fazer a previsão. Tente novamente.', 'error');";
  html += "}";
  html += "} catch (e) {";
  html += "showNotification('❌ Erro ao fazer a previsão. Tente novamente.', 'error');";
  html += "}";
  html += "})";
  html += ".finally(function() {";
  html += "btn.disabled = false;";
  html += "btnText.style.display = 'inline';";
  html += "btnLoading.style.display = 'none';";
  html += "});";
  html += "};";
  
  html += "function showResult(prediction) {";
  html += "var cropNames = ['Milho', 'Batata', 'Arroz', 'Trigo'];";
  html += "var cropEmojis = ['🌽', '🥔', '🌾', '🌾'];";
  html += "var resultDisplay = document.getElementById('resultDisplay');";
  html += "resultDisplay.innerHTML = '<div class=\"result-display\">';";
  html += "resultDisplay.innerHTML += '<h4>' + cropEmojis[prediction.cropType] + ' Previsão para ' + cropNames[prediction.cropType] + '</h4>';";
  html += "resultDisplay.innerHTML += '<div class=\"value\">' + prediction.result.toFixed(2) + '</div>';";
  html += "resultDisplay.innerHTML += '<div class=\"unit\">toneladas por hectare</div>';";
  html += "resultDisplay.innerHTML += '</div>';";
  html += "resultDisplay.style.display = 'block';";
  html += "}";
  
  html += "function updateHistory() {";
  html += "var historyList = document.getElementById('historyList');";
  html += "var cropNames = ['Milho', 'Batata', 'Arroz', 'Trigo'];";
  html += "var cropEmojis = ['🌽', '🥔', '🌾', '🌾'];";
  html += "if (predictions.length === 0) {";
  html += "historyList.innerHTML = '<p style=\"text-align: center; color: #666; padding: 40px 20px; font-size: 14px;\">🌱 Ainda não há previsões.<br>Faça sua primeira previsão para começar.</p>';";
  html += "return;";
  html += "}";
  html += "var historyHTML = '';";
  html += "for (var i = predictions.length - 1; i >= 0; i--) {";
  html += "var pred = predictions[i];";
  html += "historyHTML += '<div class=\"history-item\">';";
  html += "historyHTML += '<div class=\"history-crop\">' + cropEmojis[pred.cropType] + ' ' + cropNames[pred.cropType] + '</div>';";
  html += "historyHTML += '<div class=\"history-date\">' + pred.timestamp.toLocaleDateString('es-ES') + ' - ' + pred.timestamp.toLocaleTimeString('es-ES', {hour: '2-digit', minute: '2-digit'}) + '</div>';";
  
  // NUEVA ESTRUCTURA HTML MÁS ESTABLE:
  html += "historyHTML += '<div class=\"history-data\">';";
  html += "historyHTML += '<div class=\"history-inputs\">';";
  html += "historyHTML += '<span style=\"display: inline-block; min-width: 45px;\">🌡️ ' + pred.temperature.toFixed(1) + '°C</span> ';";
  html += "historyHTML += '<span style=\"display: inline-block; min-width: 45px;\">💧 ' + pred.humidity.toFixed(1) + '%</span> ';";
  html += "historyHTML += '<span style=\"display: inline-block; min-width: 45px;\">⚗️ pH ' + pred.ph.toFixed(1) + '</span>';";
  html += "historyHTML += '</div>';";
  html += "historyHTML += '<div class=\"history-result\">📈 ' + pred.result.toFixed(2) + ' ton/ha</div>';";
  html += "historyHTML += '</div>';";
  html += "historyHTML += '</div>';";
  html += "}";
  html += "historyList.innerHTML = historyHTML;";
  html += "}";
  
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "selectCrop(0);";
  html += "showNotification('🌱 CropAssist iniciado com sucesso. ¡Pronto para previsões!');";
  html += "});";
  html += "</script>";
  html += "</body>";
  html += "</html>";

  return html;
}

void handleRoot() {
  server.send(200, "text/html", getMainHTML());
}

float getMinimumHumidity(int cropType, float temperature) {
  const CropValidationRange* ranges;
  int rangesCount;
  
  // Seleccionar las rangos según el tipo de cultivo
  switch (cropType) {
    case 0: // Milho (Maíz)
      ranges = maizeRanges;
      rangesCount = maizeRangesCount;
      break;
    case 1: // Batata (Papa)
      ranges = potatoRanges;
      rangesCount = potatoRangesCount;
      break;
    case 2: // Arroz
      ranges = riceRanges;
      rangesCount = riceRangesCount;
      break;
    case 3: // Trigo
      ranges = wheatRanges;
      rangesCount = wheatRangesCount;
      break;
    default:
      return 0.0; // Sin restricción para cultivos no definidos
  }
  
  // Buscar el rango que corresponde a la temperatura
  for (int i = 0; i < rangesCount; i++) {
    if (temperature >= ranges[i].minTemp && temperature <= ranges[i].maxTemp) {
      return ranges[i].minHumidity;
    }
  }
  
  // Si la temperatura está fuera de todos los rangos definidos
  if (temperature < ranges[0].minTemp) {
    return ranges[0].minHumidity; // Usar primera regla si está muy frío
  } else {
    return ranges[rangesCount - 1].minHumidity; // Usar última regla si está muy caliente
  }
}

String getTemperatureRangeInfo(int cropType, float temperature) {
  const CropValidationRange* ranges;
  int rangesCount;
  
  switch (cropType) {
    case 0: ranges = maizeRanges; rangesCount = maizeRangesCount; break;
    case 1: ranges = potatoRanges; rangesCount = potatoRangesCount; break;
    case 2: ranges = riceRanges; rangesCount = riceRangesCount; break;
    case 3: ranges = wheatRanges; rangesCount = wheatRangesCount; break;
    default: return "";
  }
  
  for (int i = 0; i < rangesCount; i++) {
    if (temperature >= ranges[i].minTemp && temperature <= ranges[i].maxTemp) {
      return "Rango: " + String(ranges[i].minTemp, 1) + "°C - " + String(ranges[i].maxTemp, 1) + "°C";
    }
  }
  
  return "Fuera de rango óptimo";
}

String getCropName(int cropType) {
  switch (cropType) {
    case 0: return "Milho";
    case 1: return "Batata";
    case 2: return "Arroz";
    case 3: return "Trigo";
    default: return "Desconhecido";
  }
}

void handlePredict() {
  if (server.hasArg("c") && server.hasArg("t") && server.hasArg("h") && server.hasArg("p")) {
    int cropType = server.arg("c").toInt();
    float temperature = server.arg("t").toFloat();
    float humidity = server.arg("h").toFloat();
    float ph = server.arg("p").toFloat();
    
    // VALIDACION PREVIA CON NUEVA LÓGICA
    float minRequiredHumidity = getMinimumHumidity(cropType, temperature);
    
    if (minRequiredHumidity > 0 && humidity < minRequiredHumidity) {
      String tempRangeInfo = getTemperatureRangeInfo(cropType, temperature);
      
      // Crear mensaje de error más detallado
      String errorMessage = "{";
      errorMessage += "\"error\": true,";
      errorMessage += "\"message\": \"⚠️ Umidade insuficiente para " + getCropName(cropType) + "\",";
      errorMessage += "\"details\": \"" + tempRangeInfo + " requiere mínimo " + String(minRequiredHumidity, 1) + "% de humedad. Actual: " + String(humidity, 1) + "%\",";
      errorMessage += "\"required_humidity\": " + String(minRequiredHumidity, 1) + ",";
      errorMessage += "\"current_humidity\": " + String(humidity, 1) + ",";
      errorMessage += "\"crop\": \"" + getCropName(cropType) + "\",";
      errorMessage += "\"temperature\": " + String(temperature, 1) + ",";
      errorMessage += "\"temperature_range\": \"" + tempRangeInfo + "\"";
      errorMessage += "}";
      
      server.send(400, "application/json", errorMessage);
      
      // Log en consola mejorado
      Serial.println("❌ Validación fallida:");
      Serial.printf("   Cultivo: %s, Temp: %.1f°C (%s)\n", getCropName(cropType).c_str(), temperature, tempRangeInfo.c_str());
      Serial.printf("   Umidade atual: %.1f%%, Mínima requerida: %.1f%%\n", humidity, minRequiredHumidity);
      
      return;
    }
    
    // Si pasa la validación, continuar con la predicción normal
    input_features[0] = cropType;
    input_features[1] = temperature;
    input_features[2] = humidity;
    input_features[3] = ph;

    // Normalizar los datos
    for (int i = 0; i < NUM_INPUTS; i++) {
      input[i] = (input_features[i] - mean[i]) / scale[i];
    }

    // Realizar predicción
    if (!tf.predict(input).isOk()) {
      server.send(500, "text/plain", "Error en la predicción");
      return;
    }

    float pred = tf.output(0);
    
    // Guardar predicción en el historial
    if (predictionCount < 10) {
      predictions[predictionCount] = {
        input_features[1], // temperature
        input_features[2], // humidity
        input_features[3], // ph
        (int)input_features[0], // crop type
        pred,
        millis()
      };
      predictionCount++;
    } else {
      for (int i = 0; i < 9; i++) {
        predictions[i] = predictions[i + 1];
      }
      predictions[9] = {
        input_features[1],
        input_features[2],
        input_features[3],
        (int)input_features[0],
        pred,
        millis()
      };
    }

    // Enviar resultado exitoso
    server.send(200, "text/plain", String(pred, 4));
    
    // Debug info
    Serial.println("✅ Predicción exitosa:");
    Serial.printf("   Cultivo: %s, Temp: %.1f°C, Umidade: %.1f%%, pH: %.1f\n", 
                  getCropName(cropType).c_str(), temperature, humidity, ph);
    Serial.printf("   Resultado: %.2f ton/ha\n", pred);
    
  } else {
    server.send(400, "text/plain", "Parámetros faltantes");
  }
}

// Endpoint para obtener historial
void handleHistory() {
  String json = "[";
  for (int i = 0; i < predictionCount && i < 10; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"temperature\":" + String(predictions[i].temperature) + ",";
    json += "\"humidity\":" + String(predictions[i].humidity) + ",";
    json += "\"ph\":" + String(predictions[i].ph) + ",";
    json += "\"cropType\":" + String(predictions[i].cropType) + ",";
    json += "\"result\":" + String(predictions[i].result) + ",";
    json += "\"timestamp\":" + String(predictions[i].timestamp);
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Serial.println("🚀 Iniciando CropAssist ESP32 - Diseño Lado a Lado...");
  
  initWiFi();

  // Configurar modelo TensorFlow
  tf.setNumInputs(NUM_INPUTS);
  tf.setNumOutputs(NUM_OUTPUTS);
  tf.resolver.AddFullyConnected();
  tf.resolver.AddRelu();
  tf.resolver.AddAdd();
  tf.resolver.AddReshape();

  while (!tf.begin(crop_yield_model_tflite).isOk()) {
    Serial.println("❌ Error cargando modelo TensorFlow Lite");
    delay(1000);
  }
  Serial.println("✅ Modelo TensorFlow Lite cargado exitosamente");

  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/predict", handlePredict);
  server.on("/history", handleHistory);
  
  // Iniciar servidor
  server.begin();
  Serial.println("✅ Servidor web iniciado");
  Serial.printf("🌐 Accede a: http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("📱 CropAssist listo para funcionar!");
}

void loop() {
  server.handleClient();
  
  // Mostrar estadísticas cada 30 segundos
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    Serial.println("\n📊 Estado del Sistema:");
    Serial.printf("   WiFi: %s (RSSI: %ddBm)\n", 
                  WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado", 
                  WiFi.RSSI());
    Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   Memoria libre: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("   Predicciones realizadas: %d\n", predictionCount);
    Serial.printf("   Uptime: %lu segundos\n", millis() / 1000);
    
    if (predictionCount > 0) {
      Serial.println("   📈 Última predicción:");
      int lastIdx = predictionCount > 10 ? 9 : predictionCount - 1;
      String cropNames[] = {"Maíz", "Papa", "Arroz", "Trigo"};
      Serial.printf("      Cultivo: %s\n", cropNames[predictions[lastIdx].cropType].c_str());
      Serial.printf("      Resultado: %.2f ton/ha\n", predictions[lastIdx].result);
    }
  }
}
