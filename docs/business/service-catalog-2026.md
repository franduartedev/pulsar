# Solutions Tech — Catálogo de servicios y ofertas 2026

> Compañero de `docs/business/solutions-tech-fd-labs-masterplan-2026.md` (PARTE 2 y PARTE 6).
> Demanda real AR/LatAm 2025–2026, competencia, ticket, entrega, margen y veredicto por categoría,
> más las 6 ofertas productizadas cerradas.
>
> **Leyenda:** `HECHO` = verificado en fuente recuperada · `OPINIÓN` = afirmación de un tercero ·
> `INFERENCIA` = razonamiento propio · `RECOMENDACIÓN` = propuesta operativa · `WEAK` = fuente débil
> (vendor blog, snippet, sin método). Los tickets son rangos de mercado AR verificados por pricing
> pages de competencia; no son precios transaccionados.
>
> **Unidades:** USD = referencia de mercado; aplicar la política ARS/USD del masterplan PARTE 7.

---

## 1. Las 16 categorías — tabla maestra

| # | Categoría | Demanda (evidencia) | Competencia | Ticket 2025-26 | Entrega (2 pers.) | Margen | Mantenimiento | Riesgo | Repetibilidad | Productizable | **Veredicto** |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Webs institucionales | Alta (Reddit AR + pricing pages) | Muy alta | ARS 250k–700k / USD 150–500 | 1–3 sem | Alto | Bajo (+ARS 49,9k–130k/mes opc.) | Bajo | Alta | Muy alta | **ATTACK** |
| 2 | Landings | Alta (r/devsarg: "landing 200 USD") | Alta | ARS 240k–350k / USD 150–300 | 3–7 días | Muy alto | Nulo–bajo | Bajo | Muy alta | Muy alta | **ATTACK** |
| 3 | Reservas / turnos (producto propio) | Alta pero capturada por SaaS gratis | Muy alta (ReservaSimple, Booksolut, Gendu, Aturna…) | SaaS ARS 0–17k/mes vs a medida inviable | 1–2 meses | Bajo | Alto (WhatsApp/pagos) | Alto | Media | Baja | **AVOID** |
| 4 | Automatizaciones | Alta (Reddit: bots, Excel→automático) | Media (agencias n8n nacientes) | USD 1.500–5.000/proy (ref. €1.500) | 1–4 sem | Alto | Alto/bueno (abono) | Medio | Alta | Muy alta | **ATTACK** |
| 5 | Dashboards internos | Media-alta (57% pymes no mide impacto) | Media-alta y creciente | USD 1.000–1.800 setup | 2–5 sem | Alto | Medio | Medio | Media-alta | Media | **TEST** |
| 6 | Pequeños CRM | Alta (Reddit: CRM + WhatsApp) | Muy alta abajo (HubSpot Free, Zoho USD 14) | Setup USD 1.300–4.300 / a medida USD 15k–45k | 1–3 sem (setup) | Muy alto | Medio | Bajo–alto según build | Alta | Muy alta | **TEST** |
| 7 | Gestión de leads | Alta pero con desconfianza (r/argentina) | Alta, saturada y "scammy" | USD 300–1.500/mes (`WEAK`) | Continuo | Alto | Medio | Alto (reputación) | Media | Media | **TEST** |
| 8 | Cotizaciones | Atendida por freemiums | Alta (CotizzaPro, Presupuesto Simple…) | ≈$0 solo; USD 2k–5k embebido | 1–2 sem | Alto solo embebido | Bajo | Alto vs gratis | Media | Media | **AVOID** |
| 9 | Seguimiento de clientes | Alta (mismo dolor que CRM) | Alta (SaaS) | USD 800–3.000 setup | 1–2 sem | Alto | Medio | Bajo–medio | Alta | Alta | **TEST** |
| 10 | Inventarios simples | Alta y vieja (Reddit) pero resuelta por ERPs baratos | Muy alta (Xubio ARS 43,6k/mes, Odoo USD 7,25, Tango Nube) | A medida USD 30k+ → inviable | 3–6 sem | Bajo–medio | Medio | Alto (ARCA) | Media | Baja | **AVOID** |
| 11 | Portales internos | Media (oferta activa = mercado: Granweb, Ennoble…) | Media | USD 4.000–15.000 | 1–3 meses | Alto | Medio-alto | Medio (venta larga) | Media | Media-alta | **TEST** |
| 12 | Reporting | Media (mismo "no mide") | Media-alta + gratis en ERPs | USD 500–3.000 + abono | 1–3 sem | Alto | Medio | Medio (valor percibido) | Alta | Alta | **TEST** |
| 13 | Integraciones | Alta (vendida por ERPs, falta para sistemas chicos) | Media | USD 800–5.000/integración | 3–10 días | Muy alto | Alto/bueno | Medio (APIs) | Muy alta | Muy alta | **ATTACK** |
| 14 | Soporte técnico | Alta (USD 150–900/mes según Technosupport) | Alta (10+ MSPs) | Mantenimiento web ARS 49,9k–130k/mes | Continuo | Alto en web, bajo con guardias | Es el producto | Medio | Alta | Alta | **TEST** |
| 15 | Software a medida pequeño | Alta (Reddit + Deepyze USD 6k–14k) | Alta-dispersa | USD 6.000–28.000 (+USD 200–800/mes) | 2–4 meses | Alto | Alto (MRR) | Medio-alto | Media | Media | **ATTACK** |
| 16 | Migraciones / reemplazo de workflow manual | Alta (Excel→software, patrón repetido) | Media, poco empaquetada | USD 2.000–8.000 | 2–6 sem | Alto | Bajo–medio | Medio | Muy alta | Alta | **ATTACK** |

---

## 2. ATTACK (6) — cómo venderlo

**El bloque de valor real:** *automatizaciones + integraciones + migración del Excel + software a
medida chico*. Ticket USD 1.500–14.000, entrega 2–6 semanas, con mantenimiento USD 200–800/mes o
ARS 50–130k/mes. Ahí un equipo de 2 tiene margen, recurrencia y poca competición *bien
empaquetada* (`INFERENCIA` sobre evidencia de precios y foros).

| Ataque | Angulo de venta | Gancho |
|---|---|---|
| **Landings** | Producto de primera compra, alcance cerrado, precio publicado | "Landing publicada en 7 días, ARS X" |
| **Webs institucionales** | Puerta de entrada a mantenimiento (todo cierre ofrece Care) | Paquete por rubro con caso publicado |
| **Automatizaciones** | Mejor combinación ticket/velocidad/recurrencia | Diagnóstico: "¿dónde perdés horas por semana?" → USD 1.500–5.000, 1–4 semanas |
| **Integraciones** | Margen >75%, mejor recurrencia del catálogo ("algo siempre se rompe") | WhatsApp→CRM, MP→planilla, form→alerta: USD 800–5.000 |
| **Migraciones/workflow** | **Mejor gancho comercial:** diagnóstico pago → propuesta fija | "Te saco el Excel de encima" en 2–6 semanas, USD 2.000–8.000 |
| **Software a medida chico** | El core que justifica el ticket del equipo; MRR real con mantenimiento | Sistema simple USD 6.000–14.000 + USD 200–800/mes (Deepyze) |

**Contexto (`HECHO`, vendor studies — `WEAK` en método):** 88% de PyMEs AR declara intención de
invertir en digitalización, 68% dice que la barrera es falta de conocimiento/personal (Movistar
Empresas); 41,6% ya usa IA pero 57% no mide impacto (1ª Encuesta Nacional AR). `INFERENCIA`: el
comprador es hiper-práctico, desconfiado del marketing, con venta por WhatsApp y operación en
Excel → alcance cerrado + precio publicado + respuesta por WhatsApp.

---

## 3. TEST (7) — condición: vender como módulo/implementación, no como producto propio

| Test | Cómo venderlo | Umbral para dejar de "testearlo" |
|---|---|---|
| **Dashboards internos** | Módulo dentro de proyectos; Looker Studio/Metabase por rubro | 2–3 casos publicados → sube a oferta fija (Internal Dashboard) |
| **Pequeños CRM** | Implementación + WhatsApp + migración desde Excel sobre HubSpot/Zoho/Make (el 80% se resuelve con HubSpot Free o Zoho — Deepyze) | Nunca CRM desde cero (USD 15k–45k = ciclo largo). Cierre con retainer de ajustes |
| **Gestión de leads** | Módulo: captación → WhatsApp → CRM/automatización. **Nunca prometer volumen** de leads | Alcanza con banda de resultados descrita; riesgo reputacional alto si se promete cantidad |
| **Seguimiento de clientes** | Pegado a automatizaciones (mismo dolor, misma venta) | Se vende como parte del paquete Lead Pipeline |
| **Portales internos** | Con caso de referencia y decisión clara del decisor; venta larga | 1 caso → escala a oferta de rama; sin caso, no se prueba a ciegas |
| **Reporting** | Add-on recurrente de automatizaciones (informe semanal automático) | Nunca como servicio de entrada (valor percibido bajo) |
| **Soporte técnico** | **Solo sobre tu base instalada**: mantenimiento web/apps de tus clientes. No intentar ser MSP contra 10 incumbentes | Escala a planes Care (ver masterplan PARTE 12) |

---

## 4. AVOID (3) — por qué y la excepción

| Evitar | Motivo | Excepción (cómo ganar igual) |
|---|---|---|
| **Reservas/turnos como producto propio** | Competís contra $0 (ReservaSimple free/ARS 17k, Booksolut free, Gendu, Calendly) + alta mantención (WhatsApp API, pagos) | **Booking Setup** como servicio: configurar herramientas de terceros llave en mano (Oferta 2) |
| **Cotizaciones como producto propio** | Freemium agresivo (CotizzaPro, Presupuesto Simple free, Cotizarg) + es feature de CRM/ERP | **Template** de cotizador one-shot o módulo dentro de un proyecto mayor |
| **Inventarios simples como producto propio** | Resuelto y barato (Xubio ARS 43,6k/mes, Odoo USD 7,25, Líder licencia única) + riesgo ARCA | Solo **implementación/comisión de Xubio u Odoo**; o módulo de stock dentro de un sistema mayor |

**También AVOID (no son categorías sino combinaciones):** presupuestos abiertos, proyectos sin
anticipo, "revisiones ilimitadas", soporte 24/7 sin guardia — ver masterplan PARTE 3 y 7.

---

## 5. Las 6 ofertas productizadas

Todas: **una página con precio, alcance, exclusiones, tiempo y "no incluye"**. Publicar el precio
fijo es ventaja en una región donde "consultar precio" es la norma (`RECOMENDACIÓN`).

### Oferta 1 — Operations Map (diagnóstico pagado — puerta de entrada)

- **Cliente objetivo:** dueño de PyME 5–50 que "sabe que algo está roto" pero no sabe por dónde
  empezar (patrón: el negocio corre sobre la memoria del dueño).
- **Resultado:** documento con **10 fugas de workflow costeadas** (USD/horas), top-3 priorizado y
  plan de ataque. El cliente **se queda con el documento aunque no contrate**.
- **Alcance:** call de 90 min + 5 días async + informe escrito + sesión de revisión de 45 min.
- **NO incluye:** implementación, compras de software, estudios de mercado, más de una unidad/sucursal.
- **Tiempo:** 7–10 días.
- **Dependencia del cliente:** 1 persona que hace el proceso + ejemplos reales (planillas, chats tachados).
- **Precio:** USD 300–450.
- **Costos:** ~8 h.
- **Mantenimiento:** ninguno (one-shot).
- **Upsells:** directo a Ofertas 2/3/5/6, con descuento del 100% del audit si contratan en 30 días.

### Oferta 2 — Booking Setup ("tu agenda que reserva sola")

- **Cliente objetivo:** peluquerías, consultorios, gimnasios, veterinarias con ≥1 profesional y
  30+ turnos/semana (ICP de alojamientos también aplica para reservas de habitación).
- **Resultado:** reserva sola 24/7 desde link en WhatsApp, recordatorio automático, seña por
  MercadoPago, cero doble booking.
- **Alcance:** diagnóstico 30 min · WhatsApp Business/API · catálogo de servicios y duraciones ·
  conexión a Calendar · flujo de recordatorios · link + QR · capacitación 60 min · 15 días de soporte.
- **NO incluye:** app propia, sitio multipágina, pauta, integración con facturación/ARCA, soporte
  ilimitado, rediseño de marca.
- **Tiempo:** 7 días (calibrado por Expreso IA "setup a producción en 7 días" — `HECHO`).
- **Dependencia:** número de WhatsApp Business, lista de servicios/precios/horarios, cuenta
  MercadoPago, 1 persona disponible en 2 sesiones.
- **Precio:** USD 490 setup + USD 89/mes (ancla Expreso IA 400+80; TurnoYa USD 29–79/mes con setup 50).
- **Costos:** herramientas USD 15–40/mes (BSP/Make/API), 6–8 h de entrega; margen >70% al 3er cliente.
- **Mantenimiento:** USD 89/mes = ajustes de horario, soporte, 1 mejora pequeña/mes.
- **Upsells:** seña + recordatorio avanzado (+USD 30/mes) · mini-CRM de fichas · plantilla de
  reactivación de inactivos por WhatsApp.

### Oferta 3 — Lead Pipeline ("ningún lead se enfría")

- **Cliente objetivo:** comercios, inmobiliarias, distribuidoras con leads por IG + WhatsApp + web
  que viven en el chat del celular.
- **Resultado:** todo lead en pipeline con etapa, dueño y próxima acción; alerta si nadie contestó
  en X horas.
- **Alcance:** mapeo de canales · formulario/botón WhatsApp con contexto · unificación a
  planilla/CRM simple · 4 etapas (nuevo/contactado/propuesta/ganado-perdido) · recordatorios de
  seguimiento · dashboard básico · training.
- **NO incluye:** pauta y generación de tráfico, call center, integraciones con ERPs, migración de
  bases históricas sucias (>500 contactos = cot aparte), copywriting de campañas.
- **Tiempo:** 10 días hábiles.
- **Dependencia:** accesos a canales, catálogo/precios, definición de "lead calificado", 1
  responsable comercial que responda.
- **Precio:** USD 700–900 setup + USD 100/mes (ancla Expreso IA 600+120).
- **Costos:** ~USD 20–50/mes de herramientas + 10–12 h iniciales.
- **Mantenimiento:** USD 100/mes = revisión mensual de conversión + ajuste de secuencias.
- **Upsells:** respuestas asistidas por IA en horario inmediato · análisis de chats de WhatsApp para
  detectar leads perdidos · reporte mensual de conversión por canal.

### Oferta 4 — Quote Engine ("cotizaciones en 3 minutos, con seguimiento")

- **Cliente objetivo:** gremios, constructoras chicas, servicios profesionales, imprentas
  (ICP 3) que cotizan en Excel.
- **Resultado:** cotización PDF profesional en <3 min desde el celular, con estado y recordatorio
  automático D+2/D+5 al prospecto.
- **Alcance:** plantilla con identidad del cliente · catálogo de ítems/precios · 1 ronda de
  branding · pipeline de estados · secuencia de seguimiento · migración de 20 cotizaciones
  históricas · guía de uso.
- **NO incluye:** firma digital avanzada, facturación electrónica, e-commerce, IA de presupuestos
  por obra, mantenimiento de precios del cliente.
- **Tiempo:** 5 días hábiles.
- **Dependencia:** logo, datos bancarios/fiscales, lista de servicios y precios, 1 persona que
  valide el PDF final.
- **Precio:** USD 390 setup (+ plantilla premium one-shot opcional USD 120).
- **Costos:** 4–6 h + ~USD 10–20/mes de herramienta si usan SaaS de terceros.
- **Mantenimiento:** opcional USD 49/mes (actualización de catálogo, 2 cambios/mes).
- **Upsells:** calculadora de ahorro/ROI en la landing · integración WhatsApp "cotización
  enviada/leída" · kit proposal→factura.

### Oferta 5 — Internal Dashboard ("el número del mes, el día 1")

- **Cliente objetivo:** dueño de PyME 5–50 con datos en 3–5 herramientas y contadores externos
  (patrón: reportes copiando de sistema a sistema).
- **Resultado:** una pantalla con ventas, cobros, stock crítico y caja proyectada, actualizada sola;
  alerta por email/WhatsApp cuando un KPI cruza un umbral.
- **Alcance:** auditoría de fuentes (hasta 3) · workshop de 5–8 KPIs · conexión a planillas/POS/
  contabilidad · tablero web desplegado · alertas · doc + training · 2 rondas.
- **NO incluye:** reemplazo de ERP/contabilidad, data warehouse, forecasting con IA, soporte 24/7,
  >3 fuentes (cot aparte), hosting enterprise.
- **Tiempo:** 2–3 semanas.
- **Dependencia:** accesos de solo lectura, 1 workshop de KPIs, 1 persona que responda "¿de dónde
  sale este número?".
- **Precio:** USD 1.200–1.800 setup + USD 60/mes (anclas: Leon Builds 750, Anchormont 2.299,
  PlanManCorp 800–2.500, MasterWeb 899 — `HECHO` pricing pages).
- **Costos:** 20–30 h iniciales + hosting USD 10–30/mes.
- **Mantenimiento:** USD 60/mes = uptime, parches, 1 ajuste de KPI/mes → **camino a FUTURE SAAS**
  (patrón del masterplan PARTE 5/13).
- **Upsells:** reporte PDF semanal automático · segunda vista por rol (vendedor/deposito) ·
  Operations Map pagado como auditoría (USD 300–500).

### Oferta 6 — Cobranza Automática ("recordatorios que salen solos")

- **Cliente objetivo:** servicios B2B y distribuidoras con 10–50 facturas abiertas y el dueño
  persiguiendo por WhatsApp (2–3 h/semana solo mandando recordatorios — `OPINIÓN` de r/smallbusiness).
- **Resultado:** secuencia D-3/D0/D+3/D+7/D+14 enviada sin que nadie la escriba; lista diaria de
  "a quién perseguir hoy".
- **Alcance:** mapeo de estados de cobro · secuencia con tonos por email y WhatsApp · conexión con
  planilla o facturación existente · escalamiento a humano con template · dashboard de aging · training.
- **NO incluye:** cobranza judicial/extrajudicial, call center, descuento de facturas, integraciones
  con ERPs complejos, **garantía de cobro**.
- **Tiempo:** 7 días.
- **Dependencia:** planilla/factura exportada, reglas de escalado, aprobación de tono, contactos válidos.
- **Precio:** USD 550 setup + USD 89/mes.
- **Costos:** 6–8 h + Twilio/Make ~USD 10–30/mes según volumen.
- **Mantenimiento:** USD 89/mes = plantillas, volumen, 1 reporte mensual de DSO.
- **Upsells:** plan de pagos automático para morosos críticos · dashboard de aging por cliente ·
  integración con el Internal Dashboard.
- **[REQUIERE CONTADOR]** para cualquier automatización con montos, impuestos o comprobantes.

---

## 6. Cadencia de producto (cómo un proyecto se vuelve oferta/SaaS)

1. Cada proyecto cierra con la **checklist de salida** (masterplan PARTE 5.4): ¿qué se repite? ¿qué
   es del cliente? ¿qué datos toqué? ¿qué falta para que un tercero lo entregue en 2 semanas?
2. **3er uso** de un componente → repo de módulos.
3. **3er entregado sin personalizar** → plantilla en venta.
4. **5to con soporte que quema horas** → candidato a SaaS (con los 7 checks del masterplan PARTE 16).
5. **Prohibido:** SaaS de agenda, inventario, cotizaciones o cobranza genérico; construir por pedido
   de un solo cliente.

**Orden de lanzamiento (`RECOMENDACIÓN`):** Operations Map → Booking Setup y Cobranza (entregas de
7 días, casos rápidos) → Internal Dashboard (ticket alto).

---

## 7. Precios de referencia verificados (para calibrar)

| Ítem | Fuente (recuperada) | Rango |
|---|---|---|
| Landing | homework.com.ar, estudiocreativo.agency | ARS 850.000+ / USD 500–1.500 en agencia |
| Sitio PyME 3–8 páginas | homework.com.ar, sodi.com.ar | ARS 1.200.000–2.500.000 (agencia 2–5 pers.: ARS 400k–900k — rangos divergentes) |
| E-commerce | homework.com.ar | desde ARS 3.000.000 |
| Mantenimiento web | sodi.com.ar | ARS 25.000–150.000/mes (básico 25–50k; profesional 80–150k) |
| Hosting cliente | sodi.com.ar | ARS 5.000–40.000/mes; dominio .com.ar gratis vía NIC |
| Setup automatización AR | ia.expresoit.com | USD 400–600 setup + 80–120/mes |
| Dashboards (US) | leonbuilds.org, anchormont.com, planmancorp.com | USD 750–9.999 por proyecto |
| Landing sprint (US/intl) | norml.studio, humynixai.com | USD 1.500–6.000 flat |
| Productized premium (US) | designjoy.co | USD 4.995/mes (tier único, 48 h) |
| Gimnasios (vertical) | gymhero.fitness | ARS 20.000–80.000/mes |
| Talleres | Vehix (snippet) | desde ARS 40.000/mes |
| Inmobiliarias | Ubiquo (snippet) | ARS 31.800–79.900/mes |
| Alojamientos | roomy.com.ar, alojar.app | USD 50–100/mes · ARS 9.990–24.990/mes |
| Distribuidoras | GenuinoSoft (snippet) | ARS 28.000–42.000/mes |

**INFERENCIA:** el ticket AR aguanta **USD 300–1.800 de setup** + retainer chico mucho mejor que
los USD 5–6k de Designjoy (venden a startups US). El margen se gana por **reutilización del
módulo**, no por subir precio.

---

## Método y caveats

- Investigación de 16 categorías + ofertas productizadas, evidencia preferente 2025–2026.
- Fuentes: Reddit (vía snippets porque el fetch directo devolvió 403), pricing pages verificadas por
  fetch, foros AR, blogs de vendors, HN Algolia API. `websearch` falló con 401 en varias corridas.
- Product Hunt/listicles no se usaron como prueba de dolor; pricing pages solo como evidencia de
  **competencia y precio publicado**, no de venta real (`WEAK` en todo caso).
- Los tickets son bandas de mercado, no precios cerrados: validar con las entrevistas del
  masterplan (UNKNOWNS) antes de fijar tarifario final.
