#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <math.h>

// --- Wi-Fi tarmoq nomi va paroli ---
const char* ssid     = "Redmi 13C";
const char* password = "12345678";

// --- Vercel AI Diagnostika manzili ---
const char* VERCEL_API_URL = "https://alfa-vercel-ai.vercel.app/api/diagnose";

// --- Pinlar ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define ACS712_PIN 34

#define LED_GREEN  25
#define LED_YELLOW 26
#define LED_RED    27

// --- Mator va kalibrlash parametrlari ---
const float MOTOR_VOLTAGE = 6.0;
const float ACS712_SENSITIVITY = 0.185; // 5A: 0.185 | 20A: 0.100 | 30A: 0.066
const float DIVIDER_RATIO = 1.5;
const float ADC_REF_VOLTAGE = 3.3;
const int ADC_MAX = 4095;

WebServer server(80);
Adafruit_MPU6050 mpu;
DHT dht(DHTPIN, DHTTYPE);

bool greenState = false;
bool yellowState = false;
bool redState = false;

float zeroCurrentVoltage = 2.5;
float currentAmps = 0.0;
float powerWatts = 0.0;
float temperatureC = 25.0;
float vibrationMag = 0.0;
bool mpuOk = false;
bool dhtOk = true;
int dhtFailCount = 0;

// --- Baseline (normal holat) ---
float baseVib = 1.50;
float baseTemp = 25.0;
float baseCurr = 0.40;
float basePow = 2.40;
float prevTempC = 25.0;
unsigned long prevTempMs = 0;
float tempRiseRatePerMin = 0.0;

unsigned long lastSensorRead = 0;

// ====================== DASHBOARD SAHIFASI (flash xotirada) ======================
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uz">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover, interactive-widget=resizes-content">
<meta name="theme-color" media="(prefers-color-scheme: light)" content="#f2f2f7">
<meta name="theme-color" media="(prefers-color-scheme: dark)" content="#000000">
<meta name="color-scheme" content="light dark">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>Zavod</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin><link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
<style>
:root{
  --bg:#f2f2f7; --grp:#ffffff; --txt:#000; --sec:rgba(60,60,67,.6); --ter:rgba(60,60,67,.3);
  --sep:rgba(60,60,67,.18); --fill:rgba(120,120,128,.12); --glass:rgba(249,249,249,.78);
  --blue:#007aff; --green:#34c759; --orange:#ff9500; --red:#ff3b30; --purple:#af52de; --teal:#30b0c7;
  --ease:cubic-bezier(.32,.72,0,1); --spring:cubic-bezier(.34,1.4,.64,1);
}
@media (prefers-color-scheme:dark){:root{
  --bg:#000; --grp:#1c1c1e; --txt:#fff; --sec:rgba(235,235,245,.6); --ter:rgba(235,235,245,.3);
  --sep:rgba(84,84,88,.65); --fill:rgba(120,120,128,.32); --glass:rgba(22,22,24,.78);
  --blue:#0a84ff; --green:#30d158; --orange:#ff9f0a; --red:#ff453a; --purple:#bf5af2; --teal:#40c8e0;
}}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-tap-highlight-color:transparent;-webkit-text-size-adjust:100%;overscroll-behavior:none;height:100%}
body{height:100%;background:var(--bg);color:var(--txt);font-family:-apple-system,BlinkMacSystemFont,"Inter","SF Pro Text",system-ui,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;font-size:17px;letter-spacing:-.011em;font-feature-settings:"cv11","ss03";-webkit-font-smoothing:antialiased;overscroll-behavior:none}
button{font:inherit;color:inherit;background:none;border:0;cursor:pointer;touch-action:manipulation;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
button:focus-visible{outline:3px solid color-mix(in srgb,var(--blue) 60%,transparent);outline-offset:2px;border-radius:10px}
.i{width:1em;height:1em;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex:none}
.num{font-variant-numeric:tabular-nums}

/* ---------- qobiq ---------- */
#app{position:relative;height:100dvh;max-width:520px;margin:0 auto;overflow:hidden;background:var(--bg)}
@media (min-width:560px){body{background:color-mix(in srgb,var(--bg) 82%,var(--txt))}#app{box-shadow:0 0 0 .5px var(--sep),0 30px 80px rgba(0,0,0,.25)}}

/* nav bar */
.nav{position:absolute;top:0;left:0;right:0;z-index:20;height:calc(env(safe-area-inset-top,0px) + 44px);padding-top:env(safe-area-inset-top,0px);display:flex;align-items:center;justify-content:center;
  background:transparent;transition:background .25s,backdrop-filter .25s}
.nav::after{content:"";position:absolute;left:0;right:0;bottom:0;height:.5px;background:var(--sep);opacity:0;transition:opacity .2s}
.nav.sc{background:var(--glass);-webkit-backdrop-filter:saturate(180%) blur(22px);backdrop-filter:saturate(180%) blur(22px)}
.nav.sc::after{opacity:1}
.nav-title{font-size:17px;font-weight:600;opacity:0;transform:translateY(6px);transition:opacity .2s,transform .3s var(--ease)}
.nav.sc .nav-title{opacity:1;transform:none}

/* scroll */
#scroll{position:absolute;inset:0;overflow-y:auto;overflow-x:hidden;overscroll-behavior:contain;-webkit-overflow-scrolling:touch;
  padding:calc(env(safe-area-inset-top,0px) + 44px) 16px calc(49px + env(safe-area-inset-bottom,0px) + 28px)}
.view{display:none}
.view.on{display:block;animation:vin .35s var(--ease)}
@keyframes vin{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}
.large{font-size:34px;font-weight:700;letter-spacing:-.02em;line-height:1.1;padding:2px 4px 4px}
.lead{display:flex;align-items:center;gap:8px;padding:0 4px 18px;color:var(--sec);font-size:15px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--green);flex:none;transition:background .3s}
.dot.off{background:var(--red)}

/* guruh va qatorlar */
.gh{font-size:13px;color:var(--sec);padding:18px 16px 7px;font-weight:400}
.gf{font-size:13px;color:var(--sec);padding:7px 16px 0}
.group{background:var(--grp);border-radius:14px;overflow:hidden}
.row{position:relative;display:flex;align-items:center;gap:12px;padding:10px 16px;min-height:52px;width:100%;text-align:left;transition:background .12s}
.row+.row::before{content:"";position:absolute;top:0;left:60px;right:0;height:.5px;background:var(--sep)}
button.row:active{background:var(--fill)}
.rt{flex:1;min-width:0}
.rt b{display:block;font-weight:400;font-size:17px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.rt small{display:block;color:var(--sec);font-size:13px;margin-top:1px}
.rv{color:var(--sec);font-size:17px;display:flex;align-items:center;gap:7px;white-space:nowrap}
.ib{width:30px;height:30px;border-radius:8px;display:grid;place-items:center;font-size:18px;color:#fff;background:var(--c,var(--blue));flex:none}
.ib.soft{background:color-mix(in srgb,var(--c) 16%,transparent);color:var(--c);border-radius:50%}

/* hero */
.hero{display:flex;align-items:center;gap:18px;padding:18px;border-radius:22px;background:var(--grp)}
.ring{position:relative;width:116px;height:116px;flex:none}
.ring svg{width:100%;height:100%;transform:rotate(-90deg)}
.ring circle{fill:none;stroke-width:11;stroke-linecap:round}
.ring .tr{stroke:var(--fill)}
.ring .pg{transition:stroke-dasharray .8s var(--ease),stroke .4s}
.ring .c{position:absolute;inset:0;display:grid;place-content:center;text-align:center}
.ring .c b{font-size:30px;font-weight:700;letter-spacing:-.03em;line-height:1}
.ring .c small{font-size:12px;color:var(--sec);margin-top:3px}
.hero .tx{flex:1;min-width:0}
.hero h2{font-size:20px;font-weight:600;letter-spacing:-.02em}
.hero p{color:var(--sec);font-size:14px;margin-top:3px;line-height:1.35}
.pill{display:inline-flex;align-items:center;gap:6px;margin-top:10px;font-size:13px;font-weight:600;padding:5px 11px 5px 9px;border-radius:20px;color:var(--c);background:color-mix(in srgb,var(--c) 15%,transparent)}
.fan{font-size:15px}
.fan.on{animation:spin .9s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* plitkalar */
.tiles{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px}
.tile{background:var(--grp);border-radius:20px;padding:14px 14px 15px;display:flex;flex-direction:column;gap:9px;min-width:0}
.tile .hd{display:flex;align-items:center;gap:9px;color:var(--sec);font-size:14px;font-weight:500}
.tile .hd .ib{width:30px;height:30px}
.tile .v{font-size:32px;font-weight:700;letter-spacing:-.03em;line-height:1}
.tile .v small{font-size:15px;font-weight:500;color:var(--sec);margin-left:3px;letter-spacing:0}
.bar{height:5px;border-radius:3px;background:var(--fill);overflow:hidden}
.bar i{display:block;height:100%;width:0;border-radius:3px;background:var(--c);transition:width .8s var(--ease)}
.tile .ft{font-size:12px;color:var(--sec)}

/* tezkor LED */
.quick{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}
.qb{background:var(--grp);border-radius:20px;padding:14px 8px 12px;display:flex;flex-direction:column;align-items:center;gap:8px;font-size:13px;font-weight:500;transition:transform .15s var(--ease),background .25s}
.qb:active{transform:scale(.95)}
.qb .bulb{width:46px;height:46px;border-radius:50%;display:grid;place-items:center;font-size:23px;background:var(--fill);color:var(--sec);transition:background .25s,color .25s,box-shadow .25s}
.qb.on .bulb{background:var(--c);color:#fff;box-shadow:0 0 22px color-mix(in srgb,var(--c) 70%,transparent)}
.qb small{color:var(--sec);font-size:12px;font-weight:400}

/* iOS o'tkazgich */
.sw{width:51px;height:31px;border-radius:16px;background:var(--fill);position:relative;flex:none;transition:background .25s}
.sw::after{content:"";position:absolute;top:2px;left:2px;width:27px;height:27px;border-radius:50%;background:#fff;box-shadow:0 3px 8px rgba(0,0,0,.15),0 1px 1px rgba(0,0,0,.16);transition:transform .35s var(--spring),width .2s}
.sw.on{background:var(--c,var(--green))}
.sw.on::after{transform:translateX(20px)}
.sw:active::after{width:32px}
.sw.on:active::after{transform:translateX(15px)}

/* segment */
.seg{position:relative;display:grid;grid-template-columns:repeat(4,1fr);background:var(--fill);border-radius:9px;padding:2px;margin:0 0 12px}
.seg i{position:absolute;top:2px;bottom:2px;left:2px;width:calc((100% - 4px)/4);background:var(--grp);border-radius:7px;box-shadow:0 3px 8px rgba(0,0,0,.12),0 1px 1px rgba(0,0,0,.1);transform:translateX(calc(var(--i,0)*100%));transition:transform .35s var(--ease)}
@media (prefers-color-scheme:dark){.seg i{background:#636366}}
.seg button{position:relative;z-index:1;padding:6px 0;font-size:13px;font-weight:500;text-align:center}
.seg button.on{font-weight:600}

/* grafik kartasi */
.chartc{background:var(--grp);border-radius:20px;padding:16px 14px 10px}
.rd{padding:0 4px 8px}
.rd small{display:block;font-size:13px;color:var(--sec)}
.rd b{font-size:34px;font-weight:700;letter-spacing:-.03em}
.rd b span{font-size:16px;font-weight:500;color:var(--sec);margin-left:4px;letter-spacing:0}
#chart{width:100%;height:230px;display:block;touch-action:pan-y}
.stats{display:grid;grid-template-columns:repeat(3,1fr);margin-top:12px;text-align:center}
.stats div{padding:12px 4px;background:var(--grp);position:relative}
.stats div:first-child{border-radius:14px 0 0 14px}.stats div:last-child{border-radius:0 14px 14px 0}
.stats div+div::before{content:"";position:absolute;left:0;top:25%;bottom:25%;width:.5px;background:var(--sep)}
.stats small{display:block;font-size:12px;color:var(--sec)}
.stats b{font-size:20px;font-weight:600}

/* alarm */
.al-hero{display:flex;align-items:center;gap:18px;background:var(--grp);border-radius:22px;padding:18px}
.al-hero svg{width:104px;height:104px;flex:none}
.al-hero .lg{flex:1;display:flex;flex-direction:column;gap:11px;font-size:15px}
.al-hero .lg div{display:flex;align-items:center;gap:9px}
.al-hero .lg i{width:10px;height:10px;border-radius:50%;flex:none}
.al-hero .lg b{margin-left:auto;font-weight:600}
.chip{font-size:12px;font-weight:600;padding:3px 9px;border-radius:10px;color:var(--c);background:color-mix(in srgb,var(--c) 16%,transparent)}
.empty{display:flex;flex-direction:column;align-items:center;gap:8px;padding:34px 20px;color:var(--sec);font-size:15px;text-align:center}
.empty .i{font-size:40px;color:var(--green)}
.ai-txt{white-space:normal!important;overflow:visible!important;text-overflow:clip!important;line-height:1.35}

/* chiqish tugmasi */
.btn{width:100%;margin-top:18px;padding:15px;border-radius:14px;background:var(--grp);color:var(--red);font-weight:600;font-size:17px;transition:transform .15s var(--ease),background .12s}
.btn:active{transform:scale(.98);background:var(--fill)}

/* tab bar */
.tabbar{position:absolute;left:0;right:0;bottom:0;z-index:20;display:grid;grid-template-columns:repeat(5,1fr);padding:6px 4px calc(env(safe-area-inset-bottom,0px) + 2px);
  background:var(--glass);-webkit-backdrop-filter:saturate(180%) blur(22px);backdrop-filter:saturate(180%) blur(22px);border-top:.5px solid var(--sep)}
.tab{position:relative;display:flex;flex-direction:column;align-items:center;gap:3px;padding:3px 0 1px;color:var(--ter);font-size:10px;font-weight:500;letter-spacing:0;transition:color .2s}
.tab .i{font-size:26px;stroke-width:1.7;transition:transform .35s var(--spring)}
.tab:active .i{transform:scale(.88)}
.tab.on{color:var(--blue)}
.tab .bd{position:absolute;top:0;left:calc(50% + 6px);min-width:18px;height:18px;border-radius:9px;background:var(--red);color:#fff;font-size:11px;font-weight:700;display:grid;place-items:center;padding:0 5px;border:2px solid var(--bg);box-sizing:content-box;transform:scale(0);transition:transform .35s var(--spring)}
.tab .bd.on{transform:scale(1)}

@media (hover:hover) and (pointer:fine){button.row:hover{background:var(--fill)}.tab:hover{color:var(--sec)}.tab.on:hover{color:var(--blue)}}
@media (prefers-reduced-motion:reduce){*,*::before,*::after{animation:none!important;transition-duration:.01ms!important}}
</style>
</head>
<body>

<!-- Ikonlar (rasmdagi kabi chiziqli to'plam) -->
<svg width="0" height="0" style="position:absolute" aria-hidden="true">
  <symbol id="i-home" viewBox="0 0 24 24"><path d="M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8"/><path d="M3 10a2 2 0 0 1 .709-1.528l7-5.999a2 2 0 0 1 2.582 0l7 5.999A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></symbol>
  <symbol id="i-act" viewBox="0 0 24 24"><path d="M22 12h-2.48a2 2 0 0 0-1.93 1.46l-2.35 8.36a.25.25 0 0 1-.48 0L9.24 2.18a.25.25 0 0 0-.48 0l-2.35 8.36A2 2 0 0 1 4.49 12H2"/></symbol>
  <symbol id="i-temp" viewBox="0 0 24 24"><path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/></symbol>
  <symbol id="i-bolt" viewBox="0 0 24 24"><path d="M4 14a1 1 0 0 1-.78-1.63l9.9-10.2a.5.5 0 0 1 .86.46l-1.92 6.02A1 1 0 0 0 13 10h7a1 1 0 0 1 .78 1.63l-9.9 10.2a.5.5 0 0 1-.86-.46l1.92-6.02A1 1 0 0 0 11 14z"/></symbol>
  <symbol id="i-gauge" viewBox="0 0 24 24"><path d="m12 14 4-4"/><path d="M3.34 19a10 10 0 1 1 17.32 0"/></symbol>
  <symbol id="i-clock" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><path d="M12 6v6l4 2"/></symbol>
  <symbol id="i-alert" viewBox="0 0 24 24"><path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3"/><path d="M12 9v4"/><path d="M12 17h.01"/></symbol>
  <symbol id="i-trend" viewBox="0 0 24 24"><path d="M3 3v16a2 2 0 0 0 2 2h16"/><path d="m19 9-5 5-4-4-3 3"/></symbol>
  <symbol id="i-bulb" viewBox="0 0 24 24"><path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8c0 1 .2 2.2 1.5 3.5.7.7 1.3 1.5 1.5 2.5"/><path d="M9 18h6"/><path d="M10 22h4"/></symbol>
  <symbol id="i-chip" viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="2"/><rect x="8" y="8" width="8" height="8" rx="1"/><path d="M9 2v2M15 2v2M9 20v2M15 20v2M2 9h2M2 15h2M20 9h2M20 15h2"/></symbol>
  <symbol id="i-wifi" viewBox="0 0 24 24"><path d="M12 20h.01"/><path d="M2 8.82a15 15 0 0 1 20 0"/><path d="M5 12.859a10 10 0 0 1 14 0"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/></symbol>
  <symbol id="i-shield" viewBox="0 0 24 24"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/><path d="m9 12 2 2 4-4"/></symbol>
  <symbol id="i-fan" viewBox="0 0 24 24"><path d="M10.827 16.379a6.082 6.082 0 0 1-8.618-7.002l5.412 1.45a6.082 6.082 0 0 1 7.002-8.618l-1.45 5.412a6.082 6.082 0 0 1 8.618 7.002l-5.412-1.45a6.082 6.082 0 0 1-7.002 8.618l1.45-5.412Z"/><path d="M12 12v.01"/></symbol>
  <symbol id="i-cal" viewBox="0 0 24 24"><path d="M8 2v4"/><path d="M16 2v4"/><rect x="3" y="4" width="18" height="18" rx="2"/><path d="M3 10h18"/></symbol>
  <symbol id="i-bell" viewBox="0 0 24 24"><path d="M10.268 21a2 2 0 0 0 3.464 0"/><path d="M3.262 15.326A1 1 0 0 0 4 17h16a1 1 0 0 0 .74-1.673C19.41 13.956 18 12.499 18 8A6 6 0 0 0 6 8c0 4.499-1.411 5.956-2.738 7.326"/></symbol>
  <symbol id="i-bars" viewBox="0 0 24 24"><path d="M4 20V11M10 20V4M16 20v-7M22 20H2"/></symbol>
</svg>

<div id="app">
  <div class="nav" id="nav"><span class="nav-title" id="nav-title">Bosh sahifa</span></div>

  <div id="scroll">

    <!-- ===== BOSH SAHIFA ===== -->
    <section class="view on" id="v-home">
      <h1 class="large">Zavod</h1>
      <div class="lead"><span class="dot off" id="dot"></span><span id="lead">Ulanmoqda&hellip;</span></div>

      <div class="hero">
        <div class="ring">
          <svg viewBox="0 0 116 116"><circle class="tr" cx="58" cy="58" r="50"/><circle class="pg" id="h-ring" cx="58" cy="58" r="50" stroke="#34c759" stroke-dasharray="0 315"/></svg>
          <div class="c"><b class="num" id="h-val">&ndash;</b><small>Salomatlik</small></div>
        </div>
        <div class="tx">
          <h2>Mator</h2>
          <p id="h-txt">Ma'lumot kutilmoqda</p>
          <span class="pill" id="m-pill" style="--c:var(--red)"><svg class="i fan" id="fan"><use href="#i-fan"/></svg><span id="m-st">To'xtagan</span></span>
        </div>
      </div>

      <div class="tiles">
        <div class="tile" style="--c:var(--blue)"><div class="hd"><span class="ib soft"><svg class="i"><use href="#i-act"/></svg></span>Vibratsiya</div><div class="v num"><span id="v-vib">0.00</span><small>m/s&sup2;</small></div><div class="bar"><i id="b-vib"></i></div><div class="ft">Chegara 12.0</div></div>
        <div class="tile" style="--c:var(--orange)"><div class="hd"><span class="ib soft"><svg class="i"><use href="#i-temp"/></svg></span>Harorat</div><div class="v num"><span id="v-temp">0.0</span><small>&deg;C</small></div><div class="bar"><i id="b-temp"></i></div><div class="ft">Chegara 50 &deg;C</div></div>
        <div class="tile" style="--c:var(--green)"><div class="hd"><span class="ib soft"><svg class="i"><use href="#i-bolt"/></svg></span>Tok</div><div class="v num"><span id="v-cur">0.00</span><small>A</small></div><div class="bar"><i id="b-cur"></i></div><div class="ft">Chegara 1.5 A</div></div>
        <div class="tile" style="--c:var(--purple)"><div class="hd"><span class="ib soft"><svg class="i"><use href="#i-gauge"/></svg></span>Quvvat</div><div class="v num"><span id="v-pow">0.00</span><small>Vt</small></div><div class="bar"><i id="b-pow"></i></div><div class="ft">Chegara 5.5 Vt</div></div>
      </div>

      <div class="gh">LED tezkor boshqaruv</div>
      <div class="quick">
        <button class="qb" id="q-green" style="--c:var(--green)"><span class="bulb"><svg class="i"><use href="#i-bulb"/></svg></span>Yashil<small id="qs-green">O'chiq</small></button>
        <button class="qb" id="q-yellow" style="--c:var(--orange)"><span class="bulb"><svg class="i"><use href="#i-bulb"/></svg></span>Sariq<small id="qs-yellow">O'chiq</small></button>
        <button class="qb" id="q-red" style="--c:var(--red)"><span class="bulb"><svg class="i"><use href="#i-bulb"/></svg></span>Qizil<small id="qs-red">O'chiq</small></button>
      </div>

      <div class="gh">Qurilma</div>
      <div class="group">
        <div class="row"><span class="ib" style="--c:var(--teal)"><svg class="i"><use href="#i-clock"/></svg></span><div class="rt"><b>Ish vaqti</b></div><div class="rv num" id="v-up">&ndash;</div></div>
        <div class="row"><span class="ib" style="--c:var(--red)"><svg class="i"><use href="#i-alert"/></svg></span><div class="rt"><b>Faol alarmlar</b></div><div class="rv num" id="v-al">0</div></div>
      </div>
    </section>

    <!-- ===== TREND ===== -->
    <section class="view" id="v-trend">
      <h1 class="large">Trend</h1>
      <div class="lead">Oxirgi 2 daqiqa. Grafikni barmoq bilan suring.</div>
      <div class="seg" id="seg" style="--i:0">
        <i></i>
        <button class="on" data-k="vib">Vibr.</button><button data-k="temp">Harorat</button><button data-k="cur">Tok</button><button data-k="pow">Quvvat</button>
      </div>
      <div class="chartc">
        <div class="rd"><small id="rd-t">Hozir</small><b class="num"><span id="rd-v">&ndash;</span><span id="rd-u"></span></b></div>
        <canvas id="chart"></canvas>
      </div>
      <div class="stats">
        <div><small>Eng past</small><b class="num" id="st-min">&ndash;</b></div>
        <div><small>O'rtacha</small><b class="num" id="st-avg">&ndash;</b></div>
        <div><small>Eng yuqori</small><b class="num" id="st-max">&ndash;</b></div>
      </div>
    </section>

    <!-- ===== ALARM ===== -->
    <section class="view" id="v-alarm">
      <h1 class="large">Alarmlar</h1>
      <div class="lead" id="al-lead">Hammasi me'yorda</div>
      <div class="al-hero">
        <svg viewBox="0 0 110 110"><g id="al-arcs" transform="rotate(-90 55 55)"></g>
          <text x="55" y="60" text-anchor="middle" style="font-size:30px;font-weight:700;fill:var(--txt);letter-spacing:-1px" id="al-total">0</text></svg>
        <div class="lg">
          <div><i style="background:var(--red)"></i>Kritik<b class="num" id="al-c">0</b></div>
          <div><i style="background:var(--orange)"></i>Yuqori<b class="num" id="al-h">0</b></div>
          <div><i style="background:var(--teal)"></i>Ogohlantirish<b class="num" id="al-w">0</b></div>
        </div>
      </div>
      <div class="gh">So'nggi hodisalar</div>
      <div class="group" id="al-log"></div>
      <div class="gh">AI Diagnostika</div>
      <div class="group" id="ai-box"><div class="empty"><svg class="i"><use href="#i-shield"/></svg><b style="color:var(--txt);font-weight:600">AI hisoboti</b>Holat normal: tahlil talab qilinmaydi</div></div>
    </section>

    <!-- ===== LED ===== -->
    <section class="view" id="v-led">
      <h1 class="large">LED</h1>
      <div class="lead">Chiroqlarni masofadan boshqaring</div>
      <div class="group">
        <button class="row" id="r-green"><span class="ib" style="--c:var(--green)"><svg class="i"><use href="#i-bulb"/></svg></span><div class="rt"><b>Yashil LED</b><small id="t-green">O'chiq</small></div><span class="sw" id="sw-green" style="--c:var(--green)"></span></button>
        <button class="row" id="r-yellow"><span class="ib" style="--c:var(--orange)"><svg class="i"><use href="#i-bulb"/></svg></span><div class="rt"><b>Sariq LED</b><small id="t-yellow">O'chiq</small></div><span class="sw" id="sw-yellow" style="--c:var(--orange)"></span></button>
        <button class="row" id="r-red"><span class="ib" style="--c:var(--red)"><svg class="i"><use href="#i-bulb"/></svg></span><div class="rt"><b>Qizil LED</b><small id="t-red">O'chiq</small></div><span class="sw" id="sw-red" style="--c:var(--red)"></span></button>
      </div>
      <button class="btn" id="alloff">Barini o'chirish</button>
    </section>

    <!-- ===== DATCHIKLAR ===== -->
    <section class="view" id="v-sens">
      <h1 class="large">Datchiklar</h1>
      <div class="lead">Ulangan qurilmalar holati</div>
      <div class="group">
        <div class="row"><span class="ib" style="--c:var(--blue)"><svg class="i"><use href="#i-act"/></svg></span><div class="rt"><b>MPU-6050</b><small>Vibratsiya &middot; chegara 12.0 m/s&sup2;</small></div><div class="rv num"><span id="d-vib">&ndash;</span><span class="dot" id="d-vib-s"></span></div></div>
        <div class="row"><span class="ib" style="--c:var(--orange)"><svg class="i"><use href="#i-temp"/></svg></span><div class="rt"><b>DHT22</b><small>Harorat &middot; chegara 50 &deg;C</small></div><div class="rv num"><span id="d-temp">&ndash;</span><span class="dot" id="d-temp-s"></span></div></div>
        <div class="row"><span class="ib" style="--c:var(--green)"><svg class="i"><use href="#i-bolt"/></svg></span><div class="rt"><b>ACS712</b><small>Tok &middot; chegara 1.5 A</small></div><div class="rv num"><span id="d-cur">&ndash;</span><span class="dot" id="d-cur-s"></span></div></div>
      </div>
      <div class="gh">Tarmoq</div>
      <div class="group">
        <div class="row"><span class="ib" style="--c:var(--blue)"><svg class="i"><use href="#i-wifi"/></svg></span><div class="rt"><b>Wi-Fi signal</b></div><div class="rv num"><span id="d-wifi">&ndash;</span><span class="dot" id="d-wifi-s"></span></div></div>
        <div class="row"><span class="ib" style="--c:var(--teal)"><svg class="i"><use href="#i-chip"/></svg></span><div class="rt"><b>IP manzil</b></div><div class="rv num" id="d-ip">&ndash;</div></div>
        <div class="row"><span class="ib" style="--c:var(--purple)"><svg class="i"><use href="#i-clock"/></svg></span><div class="rt"><b>Ish vaqti</b></div><div class="rv num" id="d-up">&ndash;</div></div>
      </div>
    </section>

  </div>

  <nav class="tabbar" id="tabbar">
    <button class="tab on" data-v="home" aria-label="Bosh sahifa"><svg class="i"><use href="#i-home"/></svg>Bosh</button>
    <button class="tab" data-v="trend"><svg class="i"><use href="#i-trend"/></svg>Trend</button>
    <button class="tab" data-v="alarm"><svg class="i"><use href="#i-alert"/></svg>Alarm<span class="bd" id="tab-bd">0</span></button>
    <button class="tab" data-v="led"><svg class="i"><use href="#i-bulb"/></svg>LED</button>
    <button class="tab" data-v="sens"><svg class="i"><use href="#i-chip"/></svg>Datchik</button>
  </nav>
</div>

<script>
const $=id=>document.getElementById(id);
const AI_URL='https://alfa-vercel-ai.vercel.app/api/diagnose';
// [yuqori, kritik] chegaralar. Mator uchun moslab o'zgartiring.
const TH={vib:[1.5,12],temp:[40,50],cur:[0.9,1.5]};
const NAME={vib:'Vibratsiya',temp:'Harorat',cur:'Tok'};
const UNIT={vib:'m/s²',temp:'°C',cur:'A',pow:'Vt'};
const COL={vib:'--blue',temp:'--orange',cur:'--green',pow:'--purple'};
const ICON={vib:'act',temp:'temp',cur:'bolt'};
const TITLE={home:'Bosh sahifa',trend:'Trend',alarm:'Alarmlar',led:'LED',sens:'Datchiklar'};
const N=60;
const hist={vib:[],temp:[],cur:[],pow:[],t:[]};
let tab='vib',active={},log=[],scrub=null,view='home',aiKey='',aiAt=0;
const p2=n=>String(n).padStart(2,'0');
const hms=d=>p2(d.getHours())+':'+p2(d.getMinutes())+':'+p2(d.getSeconds());
const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim();
const ic=n=>'<svg class="i"><use href="#i-'+n+'"/></svg>';
const lvl=(k,v)=>v>=TH[k][1]?3:v>=TH[k][0]?2:v>=TH[k][0]*0.8?1:0;
const LVN={3:'Kritik',2:'Yuqori',1:'Ogoh.'};
const LVC={0:'--green',1:'--teal',2:'--orange',3:'--red'};
const fmtUp=s=>{const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h>0?h+' s '+p2(m)+' d':m>0?m+' d '+p2(s%60)+' s':s+' s'};
const dec=k=>k=='temp'?1:2;

/* ---- navigatsiya ---- */
function go(v){
  if(v===view)return;
  view=v;
  document.querySelectorAll('.view').forEach(e=>e.classList.toggle('on',e.id==='v-'+v));
  document.querySelectorAll('.tab').forEach(e=>e.classList.toggle('on',e.dataset.v===v));
  $('nav-title').textContent=TITLE[v];
  $('scroll').scrollTop=0;onScroll();
  if(v==='trend')requestAnimationFrame(draw);
  if(navigator.vibrate)navigator.vibrate(6);
}
document.querySelectorAll('.tab').forEach(b=>b.addEventListener('click',()=>go(b.dataset.v)));
function onScroll(){$('nav').classList.toggle('sc',$('scroll').scrollTop>34)}
$('scroll').addEventListener('scroll',onScroll,{passive:true});

/* ---- AI hisobot ---- */
async function askAI(diag){
  const box=$('ai-box');
  const motorRules=(diag&&diag.triggered_rule_ids||[]).filter(id=>id!=='R14');
  if(!motorRules.length){
    aiKey='';box.dataset.ok='';
    if(diag&&diag.triggered_rule_ids&&diag.triggered_rule_ids.includes('R14')){
      box.innerHTML='<div class="row"><span class="ib soft" style="--c:var(--orange)">'+ic('alert')+'</span><div class="rt"><b class="ai-txt">Datchik ulanishini tekshiring (R14)</b><small>MPU-6050 yoki DHT22 javob bermayapti</small></div></div>';
    }else{
      box.innerHTML='<div class="empty">'+ic('shield')+'<b style="color:var(--txt);font-weight:600">AI hisoboti</b>Holat normal: tahlil talab qilinmaydi</div>';
    }
    return;
  }
  const key=motorRules.slice().sort().join(',');
  if(Date.now()-aiAt < (key===aiKey?30000:15000))return;
  aiKey=key;aiAt=Date.now();
  if(!box.dataset.ok) box.innerHTML='<div class="row"><span class="ib soft" style="--c:var(--blue)">'+ic('chip')+'</span><div class="rt"><b>AI tahlil qilmoqda&hellip;</b><small>'+key+'</small></div></div>';
  try{
    const r=await fetch(AI_URL,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(diag)});
    const ai=await r.json();
    if(!r.ok){
      if(ai.error==='gemini_http_429'){aiAt=Date.now()+45000;if(box.dataset.ok)return;box.innerHTML='<div class="row"><span class="ib soft" style="--c:var(--orange)">'+ic('clock')+'</span><div class="rt"><b class="ai-txt">AI kvotasi vaqtincha to\'ldi</b><small>1 daqiqadan keyin qayta uriniladi</small></div></div>';return}
      box.dataset.ok='';box.innerHTML='<div class="row"><span class="ib soft" style="--c:var(--red)">'+ic('alert')+'</span><div class="rt"><b class="ai-txt">AI xatosi: '+(ai.error||r.status)+'</b></div></div>';return;
    }
    box.dataset.ok='1';
    let h='<div class="row"><span class="ib soft" style="--c:var(--blue)">'+ic('chip')+'</span><div class="rt"><b class="ai-txt">'+(ai.summary_uz||'Tahlil tayyor')+'</b><small>'+key+'</small></div></div>';
    (ai.ranked_causes||[]).slice(0,3).forEach(c=>{
      h+='<div class="row"><span class="ib soft" style="--c:var(--orange)">'+ic('alert')+'</span><div class="rt"><b>'+c.fault_id+'</b><small class="ai-txt">'+(c.evidence_uz||'')+'</small></div><span class="chip" style="--c:var(--orange)">'+c.probability_pct+'%</span></div>';
    });
    if(ai.recommended_actions_uz&&ai.recommended_actions_uz.length){
      h+='<div class="row"><span class="ib soft" style="--c:var(--green)">'+ic('shield')+'</span><div class="rt"><b>Tavsiya</b><small class="ai-txt">'+ai.recommended_actions_uz.join('; ')+'</small></div></div>';
    }
    box.innerHTML=h;
  }catch(e){
    box.innerHTML='<div class="row"><span class="ib soft" style="--c:var(--red)">'+ic('alert')+'</span><div class="rt"><b class="ai-txt">AI ulanish xatosi</b><small>'+e.message+'</small></div></div>';
  }
}

/* ---- render ---- */
function setDot(id,l){$(id).style.background='var('+LVC[l]+')'}
function bar(id,v,max){$(id).style.width=Math.max(0,Math.min(100,v/max*100))+'%'}

function render(d){
  $('v-vib').textContent=d.vibration.toFixed(2);
  $('v-temp').textContent=d.temperature.toFixed(1);
  $('v-cur').textContent=d.current.toFixed(2);
  $('v-pow').textContent=d.power.toFixed(2);
  bar('b-vib',d.vibration,TH.vib[1]);bar('b-temp',d.temperature,TH.temp[1]);bar('b-cur',d.current,TH.cur[1]);bar('b-pow',d.power,5.5);
  const up=fmtUp(d.uptime);$('v-up').textContent=up;$('d-up').textContent=up;

  const V={vib:d.vibration,temp:d.temperature,cur:d.current};
  let c=0,h=0,w=0,ratio=0;
  for(const k in V){
    const l=lvl(k,V[k]);
    if(l==3)c++;else if(l==2)h++;else if(l==1)w++;
    ratio=Math.max(ratio,Math.min(1,V[k]/TH[k][1]));
    $('d-'+k).textContent=V[k].toFixed(dec(k))+' '+UNIT[k];setDot('d-'+k+'-s',l);
    if((active[k]||0)!==l&&l>0){log.unshift({t:hms(new Date()),m:NAME[k]+' '+V[k].toFixed(dec(k))+' '+UNIT[k],k:k,l:l});log=log.slice(0,20)}
    active[k]=l;
  }
  const total=c+h+w;
  $('v-al').textContent=total;
  $('al-c').textContent=c;$('al-h').textContent=h;$('al-w').textContent=w;$('al-total').textContent=total;
  $('al-lead').textContent=total?(c?'Kritik holat aniqlandi':'Ogohlantirish bor'):"Hammasi me'yorda";
  const bd=$('tab-bd');bd.textContent=total;bd.classList.toggle('on',total>0);
  // alarm donut
  const arcs=$('al-arcs');arcs.innerHTML='';const circ=2*Math.PI*40;let off=0;
  const segs=total?[[c,css('--red')],[h,css('--orange')],[w,css('--teal')]]:[[1,css('--fill')]];
  segs.forEach(s=>{if(!s[0])return;const len=s[0]/(total||1)*circ;
    arcs.insertAdjacentHTML('beforeend','<circle cx="55" cy="55" r="40" fill="none" stroke="'+s[1]+'" stroke-width="12" stroke-linecap="butt" stroke-dasharray="'+Math.max(0,len-(total>1?2:0))+' '+circ+'" stroke-dashoffset="'+(-off)+'"/>');off+=len});
  $('al-log').innerHTML=log.length?log.map(r=>'<div class="row"><span class="ib soft" style="--c:var('+LVC[r.l]+')">'+ic(ICON[r.k])+'</span><div class="rt"><b>'+r.m+'</b><small>'+r.t+'</small></div><span class="chip" style="--c:var('+LVC[r.l]+')">'+LVN[r.l]+'</span></div>').join(''):'<div class="empty">'+ic('shield')+'<b style="color:var(--txt);font-weight:600">Hodisalar yo\'q</b>Barcha ko\'rsatkichlar chegara ichida</div>';

  // AI diagnostika
  if(d.diag)askAI(d.diag);

  // salomatlik
  const hp=Math.round((1-ratio)*100);
  $('h-val').textContent=hp+'%';
  const ring=$('h-ring');ring.setAttribute('stroke-dasharray',(hp/100*314.2)+' 315');
  ring.setAttribute('stroke',css(hp>60?'--green':hp>30?'--orange':'--red'));
  $('h-txt').textContent=hp>60?"Barcha ko'rsatkichlar yaxshi":hp>30?'Yuklama oshgan, nazorat qiling':'Xavfli holat, mator tekshirilsin';

  // mator
  const run=d.current>0.05;
  $('m-st').textContent=run?'Ishlayapti':"To'xtagan";
  $('m-pill').style.setProperty('--c',run?'var(--green)':'var(--red)');
  $('fan').classList.toggle('on',run);

  // LED
  ['green','yellow','red'].forEach(n=>{
    const on=d.leds[n];
    $('sw-'+n).classList.toggle('on',on);$('q-'+n).classList.toggle('on',on);
    $('t-'+n).textContent=on?'Yoniq':"O'chiq";$('qs-'+n).textContent=on?'Yoniq':"O'chiq";
  });

  // tarmoq
  $('d-wifi').textContent=d.rssi+' dBm';$('d-ip').textContent=d.ip;setDot('d-wifi-s',d.rssi>-70?0:d.rssi>-80?1:2);

  // tarix
  hist.vib.push(d.vibration);hist.temp.push(d.temperature);hist.cur.push(d.current);hist.pow.push(d.power);hist.t.push(hms(new Date()));
  for(const k in hist)if(hist[k].length>N)hist[k].shift();
  if(view==='trend')draw();
}

function setOnline(v){
  $('dot').classList.toggle('off',!v);
  $('lead').textContent=v?'Onlayn':'Aloqa yo\'q. Qayta urinilmoqda…';
}

/* ---- grafik (barmoq izidan 1:1 yuradi) ---- */
function draw(){
  const cv=$('chart');if(!cv.clientWidth)return;
  const dpr=window.devicePixelRatio||1,W=cv.clientWidth,H=cv.clientHeight;
  cv.width=W*dpr;cv.height=H*dpr;
  const g=cv.getContext('2d');g.scale(dpr,dpr);
  const data=hist[tab],col=css(COL[tab]),sec=css('--sec'),sep=css('--sep');
  const R=34,T=8,B=22,L=2,pw=W-L-R,ph=H-T-B;
  let mx=Math.max(...data,0.001),mn=0;
  if(tab=='temp'&&data.length){mn=Math.floor(Math.max(0,Math.min(...data)-3));mx=Math.max(mx+3,mn+5)}else mx=mx*1.25+0.05;
  g.font='11px Inter,-apple-system,Segoe UI,Arial';g.lineWidth=.5;
  for(let i=0;i<=4;i++){const y=T+ph*i/4,v=mx-(mx-mn)*i/4;
    g.strokeStyle=sep;g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();
    g.fillStyle=sec;g.textAlign='left';g.fillText(v.toFixed(mx>20?0:1),W-R+5,y+4)}
  g.textAlign='center';g.fillStyle=sec;
  [['2 daq',0],['1 daq',.5],['hozir',1]].forEach(s=>g.fillText(s[0],L+pw*s[1]-(s[1]==1?14:s[1]==0?-12:0),H-5));
  if(TH[tab]){const th=TH[tab][0];if(th>mn&&th<mx){const y=T+ph*(1-(th-mn)/(mx-mn));
    g.setLineDash([4,4]);g.strokeStyle=css('--orange');g.lineWidth=1;g.beginPath();g.moveTo(L,y);g.lineTo(W-R,y);g.stroke();g.setLineDash([])}}
  const n=data.length;
  if(n>1){
    const xs=i=>L+pw*(i+(N-n))/(N-1),ys=v=>T+ph*(1-(v-mn)/(mx-mn));
    const gr=g.createLinearGradient(0,T,0,T+ph);gr.addColorStop(0,col+'55');gr.addColorStop(1,col+'00');
    g.beginPath();g.moveTo(xs(0),ys(data[0]));
    for(let i=1;i<n;i++){const x0=xs(i-1),x1=xs(i),y0=ys(data[i-1]),y1=ys(data[i]),mx2=(x0+x1)/2;g.bezierCurveTo(mx2,y0,mx2,y1,x1,y1)}
    g.strokeStyle=col;g.lineWidth=2.5;g.lineJoin='round';g.stroke();
    g.lineTo(xs(n-1),T+ph);g.lineTo(xs(0),T+ph);g.closePath();g.fillStyle=gr;g.fill();
    const i=scrub===null?n-1:Math.min(n-1,Math.max(0,scrub));
    const px=xs(i),py=ys(data[i]);
    if(scrub!==null){g.strokeStyle=sec;g.lineWidth=1;g.beginPath();g.moveTo(px,T);g.lineTo(px,T+ph);g.stroke()}
    g.fillStyle=css('--grp');g.beginPath();g.arc(px,py,6,0,7);g.fill();
    g.fillStyle=col;g.beginPath();g.arc(px,py,4,0,7);g.fill();
    $('rd-v').textContent=data[i].toFixed(dec(tab));$('rd-u').textContent=UNIT[tab];
    $('rd-t').textContent=scrub===null?'Hozir':hist.t[i];
    const mi=Math.min(...data),ma=Math.max(...data),av=data.reduce((a,b)=>a+b,0)/n;
    $('st-min').textContent=mi.toFixed(dec(tab));$('st-avg').textContent=av.toFixed(dec(tab));$('st-max').textContent=ma.toFixed(dec(tab));
  }
}
const cv=$('chart');
function pt(e){const r=cv.getBoundingClientRect(),n=hist[tab].length,pw=r.width-36;
  const f=Math.max(0,Math.min(1,(e.clientX-r.left-2)/pw));return Math.round(f*(N-1))-(N-n)}
cv.addEventListener('pointerdown',e=>{cv.setPointerCapture(e.pointerId);scrub=pt(e);draw()});
cv.addEventListener('pointermove',e=>{if(scrub!==null){scrub=pt(e);draw()}});
['pointerup','pointercancel'].forEach(t=>cv.addEventListener(t,()=>{scrub=null;draw()}));
$('seg').addEventListener('click',e=>{
  const b=e.target.closest('button');if(!b)return;
  tab=b.dataset.k;const bs=[...$('seg').querySelectorAll('button')];
  bs.forEach(x=>x.classList.toggle('on',x===b));$('seg').style.setProperty('--i',bs.indexOf(b));draw();
});
addEventListener('resize',draw);
matchMedia('(prefers-color-scheme: dark)').addEventListener('change',()=>{draw()});

/* ---- LED boshqaruv ---- */
async function api(u){try{const r=await fetch(u);render(await r.json());setOnline(true)}catch(e){setOnline(false)}}
function tg(n){if(navigator.vibrate)navigator.vibrate(10);api('/api/toggle?led='+n)}
['green','yellow','red'].forEach(n=>{$('q-'+n).addEventListener('click',()=>tg(n));$('r-'+n).addEventListener('click',()=>tg(n))});
$('alloff').addEventListener('click',()=>api('/api/alloff'));

/* ---- ma'lumot olish ---- */
async function poll(){try{const r=await fetch('/json',{cache:'no-store'});render(await r.json());setOnline(true)}catch(e){setOnline(false)}}
poll();setInterval(poll,2000);
</script>
</body>
</html>
)rawliteral";

// ====================== SENSORLAR ======================
void applyLedStates() {
  digitalWrite(LED_GREEN, greenState ? HIGH : LOW);
  digitalWrite(LED_YELLOW, yellowState ? HIGH : LOW);
  digitalWrite(LED_RED, redState ? HIGH : LOW);
}

void calibrateCurrentZero() {
  long sum = 0;
  const int samples = 200;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }
  float avgAdc = (float)sum / samples;
  zeroCurrentVoltage = ((avgAdc / ADC_MAX) * ADC_REF_VOLTAGE) * DIVIDER_RATIO;
}

float readCurrentAmps() {
  long sum = 0;
  const int samples = 300;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delayMicroseconds(200);
  }
  float avgAdc = (float)sum / samples;
  float acsVoltage = ((avgAdc / ADC_MAX) * ADC_REF_VOLTAGE) * DIVIDER_RATIO;
  float current = (acsVoltage - zeroCurrentVoltage) / ACS712_SENSITIVITY;
  if (fabs(current) < 0.03) current = 0.0;
  return fabs(current);
}

void readSensors() {
  if (mpuOk) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float totalAccel = sqrt(a.acceleration.x * a.acceleration.x +
                            a.acceleration.y * a.acceleration.y +
                            a.acceleration.z * a.acceleration.z);
    vibrationMag = fabs(totalAccel - 9.81);
  }

  float t = dht.readTemperature();
  if (!isnan(t)) {
    dhtFailCount = 0;
    dhtOk = true;
    temperatureC = t;
    unsigned long now = millis();
    if (prevTempMs > 0 && now > prevTempMs) {
      float dtMin = (now - prevTempMs) / 60000.0;
      if (dtMin >= 0.03) {
        tempRiseRatePerMin = (temperatureC - prevTempC) / dtMin;
        prevTempC = temperatureC;
        prevTempMs = now;
      }
    } else {
      prevTempC = temperatureC;
      prevTempMs = now;
    }
  } else {
    dhtFailCount++;
    if (dhtFailCount >= 3) dhtOk = false;
  }

  currentAmps = readCurrentAmps();
  powerWatts = MOTOR_VOLTAGE * currentAmps;
}

void setBaselineFromCurrent() {
  baseVib = (vibrationMag > 0.20) ? vibrationMag : 1.50;
  baseTemp = temperatureC;
  baseCurr = (currentAmps > 0.15) ? currentAmps : 0.40;
  basePow = (powerWatts > 0.50) ? powerWatts : 2.40;
}

String buildDiagObj() {
  float vibRatio = (baseVib > 0.01) ? (vibrationMag / baseVib) : 1.0;
  float currRatio = (baseCurr > 0.01) ? (currentAmps / baseCurr) : 1.0;
  float powRatio = (basePow > 0.01) ? (powerWatts / basePow) : 1.0;
  float tempDelta = temperatureC - baseTemp;
  bool sensorOk = mpuOk && dhtOk;

  String rules = "";
  String level = "normal";
  auto addRule = [&](const char* id, const char* lvl) {
    if (rules.length() > 0) rules += ",";
    rules += "\"" + String(id) + "\"";
    if (String(lvl) == "critical") level = "critical";
    else if (String(lvl) == "alarm" && level != "critical") level = "alarm";
    else if (String(lvl) == "warning" && level != "critical" && level != "alarm") level = "warning";
    else if (String(lvl) == "watch" && level == "normal") level = "watch";
  };

  if (currRatio >= 2.0 || temperatureC >= 65.0) addRule("R15", "critical");
  if (currRatio >= 1.6 && (tempRiseRatePerMin >= 5.0 || vibRatio >= 2.0)) addRule("R01", "critical");
  if (vibRatio >= 2.0 && tempDelta >= 25.0 && currRatio >= 1.3) addRule("R11", "alarm");
  if (tempDelta >= 25.0 && currRatio >= 1.25) addRule("R07", "alarm");
  if ((currRatio >= 1.15 && powRatio >= 1.15) && !(vibRatio >= 2.0 || currRatio >= 1.6)) addRule("R08", "warning");
  if (tempDelta >= 15.0 && !(currRatio >= 1.15 || vibRatio >= 1.5)) addRule("R09", "warning");
  if (vibRatio >= 1.5 && !(tempDelta >= 15.0 || currRatio >= 1.15)) addRule("R13", "warning");
  if (!sensorOk) addRule("R14", "watch");

  String json = "{";
  json += "\"device_id\":\"DEV-ZAVOD-001\",";
  json += "\"load_mode\":\"nominal\",";
  json += "\"level\":\"" + level + "\",";
  json += "\"features_now\":{";
  json += "\"vib_rms_m_s2\":" + String(vibrationMag, 2) + ",";
  json += "\"vib_ratio\":" + String(vibRatio, 2) + ",";
  json += "\"temp_motor_C\":" + String(temperatureC, 1) + ",";
  json += "\"temp_delta_K\":" + String(tempDelta, 1) + ",";
  json += "\"temp_rise_rate_K_per_min\":" + String(tempRiseRatePerMin, 2) + ",";
  json += "\"current_A\":" + String(currentAmps, 2) + ",";
  json += "\"current_ratio\":" + String(currRatio, 2) + ",";
  json += "\"power_W\":" + String(powerWatts, 2) + ",";
  json += "\"power_ratio\":" + String(powRatio, 2) + ",";
  json += "\"sensor_ok\":" + String(sensorOk ? "true" : "false");
  json += "},";
  json += "\"triggered_rule_ids\":[" + rules + "]";
  json += "}";
  return json;
}

// --- JSON satrini tayyorlash ---
String buildJson() {
  String json = "{";
  json += "\"vibration\":" + String(vibrationMag, 2) + ",";
  json += "\"temperature\":" + String(temperatureC, 1) + ",";
  json += "\"current\":" + String(currentAmps, 2) + ",";
  json += "\"power\":" + String(powerWatts, 2) + ",";
  json += "\"uptime\":" + String(millis() / 1000UL) + ",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"leds\":{";
  json += "\"green\":" + String(greenState ? "true" : "false") + ",";
  json += "\"yellow\":" + String(yellowState ? "true" : "false") + ",";
  json += "\"red\":" + String(redState ? "true" : "false");
  json += "},";
  json += "\"diag\":" + buildDiagObj();
  json += "}";
  return json;
}

// ====================== HANDLERLAR ======================
// Katta sahifani bo'laklab (chunked) yuboradi: ERR_CONTENT_LENGTH_MISMATCH xatosini oldini oladi
void handleRoot() {
  const size_t total = strlen_P(PAGE);
  const size_t CHUNK = 1024;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");
  for (size_t i = 0; i < total; i += CHUNK) {
    size_t n = (total - i < CHUNK) ? (total - i) : CHUNK;
    server.sendContent_P(PAGE + i, n);
    delay(1);
  }
  server.sendContent("");
}

void handleJson() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildJson());
}

void handleDiag() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildDiagObj());
}

void handleBaseline() {
  readSensors();
  setBaselineFromCurrent();
  server.send(200, "application/json", buildJson());
}

void toggleByName(const String& led) {
  if (led == "green") greenState = !greenState;
  else if (led == "yellow") yellowState = !yellowState;
  else if (led == "red") redState = !redState;
  applyLedStates();
}

// Sahifani qayta yuklamasdan ishlaydigan API
void handleApiToggle() {
  if (server.hasArg("led")) toggleByName(server.arg("led"));
  server.send(200, "application/json", buildJson());
}

void handleApiAllOff() {
  greenState = yellowState = redState = false;
  applyLedStates();
  server.send(200, "application/json", buildJson());
}

// Eski havolalar ham ishlaydi
void handleToggle() {
  if (server.hasArg("led")) toggleByName(server.arg("led"));
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAllOff() {
  greenState = yellowState = redState = false;
  applyLedStates();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  applyLedStates();

  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);
  dht.begin();

  if (!mpu.begin()) {
    Serial.println("XATO: MPU-6050 topilmadi!");
    mpuOk = false;
  } else {
    mpuOk = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  delay(500);
  calibrateCurrentZero();

  Serial.print("Wi-Fi ga ulanmoqda: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi ulandi!");
  Serial.print("Brauzerda ochish uchun IP manzil: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/json", handleJson);
  server.on("/diag", handleDiag);
  server.on("/baseline", handleBaseline);
  server.on("/api/toggle", handleApiToggle);
  server.on("/api/alloff", handleApiAllOff);
  server.on("/toggle", handleToggle);
  server.on("/alloff", handleAllOff);
  server.begin();

  readSensors();
  setBaselineFromCurrent();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorRead >= 2000) {
    lastSensorRead = now;
    readSensors();
  }
}
