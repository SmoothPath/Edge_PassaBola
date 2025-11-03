//Código do arquivo "sthRoutes.js" utilizado na Plataforma Web Passa a Bola
// PassaBola/backend/routes/sthRoutes.js

const express = require("express");
const axios = require("axios");
const mqtt = require("mqtt");

const router = express.Router();

// IP da VM com STH-Comet
const BASE_URL =
  "http://20.220.27.106:8666/STH/v1/contextEntities/type/Munhequeira/id/urn:ngsi-ld:Munhequeira:001/attributes";

// ============ MQTT CONFIGURAÇÃO ============ //
const MQTT_BROKER = "mqtt://20.220.27.106:1883";
const MQTT_TOPIC_SUB = "/TEF/munhequeira001/attrs"; // Recebe dados do ESP32
const MQTT_TOPIC_PUB = "/TEF/munhequeira001/cmd"; // --- NOVO --- para enviar comandos ao ESP32

let ultimoPayloadMQTT = null; // Guarda o último dado recebido via MQTT
const client = mqtt.connect(MQTT_BROKER);

client.on("connect", () => {
  // console.log("✅ Conectado ao Broker MQTT");
  client.subscribe(MQTT_TOPIC_SUB, (err) => {
    if (!err) console.log(`📡 Inscrito no tópico: ${MQTT_TOPIC_SUB}`);
  });
});

client.on("message", (topic, message) => {
  const msg = message.toString();
  // console.log(`📥 Mensagem MQTT recebida (${topic}):`, msg);
  ultimoPayloadMQTT = msg;
});

// --- NOVO ---
// Rota para enviar comandos MQTT ao ESP32
router.post("/comando", (req, res) => {
  const { comando } = req.body;
  if (!comando) {
    return res.status(400).json({ erro: "Comando não fornecido" });
  }

  const mensagem = `munhequeira001@${comando}|`; // formato compatível com o callback do ESP32
  client.publish(MQTT_TOPIC_PUB, mensagem, { qos: 0, retain: false }, (err) => {
    if (err) {
      // console.error("❌ Erro ao enviar comando MQTT:", err); 
      return res.status(500).json({ erro: "Falha ao enviar comando MQTT" });
    }
    // console.log(`📤 Comando MQTT enviado: ${mensagem}`);
    res.json({ sucesso: true, comando: mensagem });
  });
});
// --- FIM DO NOVO ---

// ============ ROTA PRINCIPAL STH-COMET ============ //
router.get("/", async (req, res) => {
  try {
    const headers = {
      "Fiware-Service": "smart",
      "Fiware-ServicePath": "/",
    };

    const [tempRes, batRes, calRes] = await Promise.all([
      axios.get(`${BASE_URL}/temperatura?lastN=30`, { headers }),
      axios.get(`${BASE_URL}/batimento?lastN=30`, { headers }),
      axios.get(`${BASE_URL}/calorias?lastN=30`, { headers }),
    ]);

    const parseData = (res, atributo) => {
      const valores =
        res.data.contextResponses?.[0]?.contextElement?.attributes?.[0]?.values ||
        [];
      return valores.map((v) => ({
        atributo,
        value: parseFloat(v.attrValue),
        time: new Date(v.recvTime).toLocaleTimeString("pt-BR", {
          hour: "2-digit",
          minute: "2-digit",
        }),
      }));
    };

    const data = {
      temperatura: parseData(tempRes, "temperatura"),
      batimento: parseData(batRes, "batimento"),
      calorias: parseData(calRes, "calorias"),
      mqtt: ultimoPayloadMQTT || "Nenhum dado MQTT recebido ainda",
    };

    res.json(data);
  } catch (error) {
    // console.error("Erro ao buscar dados do STH-Comet:", error.message);
    res.status(500).json({ error: "Falha ao obter dados" });
  }
});

module.exports = { router };
