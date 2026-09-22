# Solutions Tech + FD Labs — Masterplan 2026

> Modelo de empresa, oferta, ventas, delivery, productos y roadmap.
> Investigación: 11 tracks en paralelo, evidencia 2025–2026, consolidada el 22-09-2026.
>
> **Leyenda de evidencia:** `HECHO` = verificado en fuente recuperada · `OPINIÓN` = afirmación de un
> tercero (foro/autor) · `INFERENCIA` = razonamiento propio · `RECOMENDACIÓN` = propuesta operativa ·
> `WEAK` = fuente débil (vendor blog, snippet, sin método). Las URLs citadas fueron recuperadas en la
> investigación; el detalle completo de fuentes vive en los 11 informes fuente de esta corrida.
>
> **Documentos hermanos:** `docs/business/service-catalog-2026.md` (catálogo de servicios y ofertas
> productizadas) y `docs/business/project-acceptance-framework.md` (matrices de aceptación y no-go).
> **Este documento no es asesoría jurídica ni impositiva** — ver PARTE 22.

---

## EXECUTIVE SUMMARY

1. **Estructura:** consolidación parcial de marca — Solutions Tech es la marca comercial y operativa;
   FD Labs es la submarca de producto con atribución visible ("por el equipo de Solutions Tech/FD
   Labs"). Dominio propio solo cuando un producto tenga pricing page y soporte propio. Máximo una
   marca nueva activa por año.
2. **La agencia paga las cuentas; el producto es opción.** Solutions Tech debe sostener ≥EXPECTED
   (≈ARS 2,4M/mes brutos) antes de que FD Labs consuma sus horas reservadas (6h/semana).
3. **Ataque (6):** webs/landings estandarizadas como producto de entrada + el bloque real de valor:
   **automatizaciones, integraciones, migraciones del Excel y software a medida chico** (ticket
   USD 1.500–14.000, entrega 2–6 semanas, con mantenimiento USD 200–800 o ARS 50–130k/mes).
   **Test (7):** dashboards, CRM, leads, seguimiento, portales, reporting, soporte-web — siempre como
   módulo/implementación hasta tener 2–3 casos. **Evitar (3):** reservas, cotizaciones, inventarios —
   SaaS gratis + MercadoPago/ARCA ya ganaron.
4. **ICP principal:** alojamiento independiente de 5–25 unidades con reservas por WhatsApp+Booking+Excel.
   Completan el top 5: inmobiliaria 30–70 contratos, imprenta chica, taller mecánico 2–6 bahías,
   distribuidora 5–20 empleados.
5. **Precio:** paquete cerrado con alcance OUT explícito, anticipo 50%, 2 rondas, cambio de alcance =
   cotización nueva, validez 15 días, referencia USD con ajuste por cláusula en ARS.
6. **Capacidad dura:** máx 3 proyectos activos (2 si alguno es custom), 15–20 clientes de
   mantenimiento, 6h/semana de producto. Regla: no se firma con 3 WIP o sin 10h libres reservadas.
7. **No-go firme (12):** ERP propio, contabilidad propia, fintech, software médico, redes sociales,
   enterprise, infraestructura crítica, scraping dudoso, APIs no oficiales, WhatsApp/Instagram no
   autorizados, 24/7, mega-fixed-price. INVESTIGATE: POS fiscal, marketplaces, integración contable.
8. **Producto:** regla del "uno" — 1 flagship (PULSAR) + 1 OSS + 1 producto pequeño + 1 SaaS en
   validación, dentro de 10–14h/semana reales de producto. SaaS solo con pago antes de la primera
   línea de código de producción.
9. **Economía honesta:** el escenario LOW no paga dos salarios. Piso de supervivencia ≈ ARS 3M/mes
   brutos (1 web + 10 mantenimientos lo alcanza).
10. **Pre-mortem:** las 12 causas de fracaso documentadas (CB Insights 431 cierres + Ignition + Reddit)
    están en PARTE 27 con early warning / prevention / response.

---

## PARTE 1 — MODELO DE EMPRESA

```
Solutions Tech (marca comercial y operativa)
├── Servicios (webs, landings, software a medida chico)
├── Automatización (workflows, integraciones, migraciones)
├── Software interno para clientes (dashboards, portales, módulos)
└── Mantenimiento (hosting, backups, updates, soporte con horario)

FD Labs (submarca de producto, atribución visible)
├── Open Source (componentes + PULSAR core)
├── Desktop Products (PULSAR flagship + apps)
├── SaaS (validación, máx 1 en el tapete)
└── Experimental / R&D (sin deadline, sin promesa)
```

### Las tres opciones evaluadas

| Opción | Ventajas comerciales | Desventajas comerciales |
|---|---|---|
| **1. Marcas separadas** | Cada marca habla su idioma; aisla riesgo reputacional. Precedente: 37signals renombró a "Basecamp" (feb-2014) para enfocarse y volvió a 37signals (may-2022) — *el nombre sigue al producto ganador* | Duplica la superficie a sostener (2 sitios, 2 redes, 2 newsletters) con 1 persona de marketing; confusión de audiencia en mercado chico; señales de precio contradictorias. Searchlab (2026): la trampa #1 de una agencia de 2 es "pretender ser más grande" |
| **2. Marcas hermanas con paraguas** | Modelo 37signals: marca casa + productos con atribución (pie de pricing de Basecamp: *"designed, built and backed by 37signals"*). Grow and Convert + Traqer.ai: producto con sitio propio, *"built by the team at…"*. Aerolab (AR) → Aerolab Ventures: separación justificada porque **cambia el modelo económico** | Requiere manual de mensajes (quién dice qué) que hoy recae 100% en Jaz; dilución de precios si el producto se regala dentro de los servicios |
| **3. Consolidación parcial ✅** | Una marca operativa + submarcas mínimas: Animalz usa subdominios (`revive.animalz.co`) → costo casi nulo; Tiptap (überdosis, 33k★, YC S23): infra compartida + marca de producto separada | Requiere disciplina de atribución; el riesgo de transitar agencia→producto es real: Philip Isik (überdosis/Tiptap, HN mar-2025): *"We nearly killed our company… juggling client deadlines with the growing needs of Tiptap was brutal"* |

### RECOMENDACIÓN

- **Opción 3.** Solutions Tech opera y vende; FD Labs es submarca con línea de atribución visible
  ("por el equipo de Solutions Tech / FD Labs"). Dominio propio solo si el producto tiene pricing
  page + soporte propio (modelo Traqer). Excepción Aerolab: marca separada solo si cambia el modelo
  económico (equity, marketplace, comunidad separada).
- **Regla de gasto:** máximo **1 marca activa nueva por año** con equipo de 2. Todo lo demás,
  submarca o subdominio.

---

## PARTE 2 — QUÉ SERVICIOS DEBE VENDER LA AGENCIA

Investigación de demanda real AR/LatAm 2025–26 por categoría (precios de competencia verificados,
Reddit/foros, pricing pages). Detalle completo en `docs/business/service-catalog-2026.md`.

| Categoría | Ticket 2025-26 | Veredicto |
|---|---|---|
| Webs institucionales | ARS 250k–700k / USD 150–500 | **ATTACK** (puerta de entrada + mantenimiento) |
| Landings | ARS 240k–350k / USD 150–300 | **ATTACK** (producto de primera compra) |
| Automatizaciones | USD 1.500–5.000/proyecto | **ATTACK** (mejor ticket/velocidad/recurrencia) |
| Integraciones | USD 800–5.000/integración | **ATTACK** (margen >75%, mejor recurrencia) |
| Migraciones / reemplazo de workflow manual | USD 2.000–8.000 | **ATTACK** (mejor gancho: diagnóstico) |
| Software a medida pequeño | USD 6.000–14.000 + USD 200–800/mes | **ATTACK** (core; justifica el ticket del equipo) |
| Dashboards internos | USD 1.000–1.800 setup | **TEST** (como módulo, hasta 2–3 casos) |
| Pequeños CRM | setup USD 1.300–4.300 | **TEST** (implementación + WhatsApp, nunca CRM desde cero) |
| Gestión de leads | USD 300–1.500/mes | **TEST** (módulo; nunca prometer volumen) |
| Seguimiento de clientes | USD 800–3.000 setup | **TEST** (pegado a automatizaciones) |
| Portales internos | USD 4.000–15.000 | **TEST** (venta larga; con caso de referencia) |
| Reporting | USD 500–3.000 + abono | **TEST** (add-on recurrente de automatizaciones) |
| Soporte técnico | Mantenimiento web ARS 49,9k–130k/mes | **TEST** (solo sobre tu base instalada; no ser MSP) |
| Reservas / turnos (producto propio) | SaaS ARS 0–17k/mes | **AVOID** (competir contra $0; revender/implementar SaaS) |
| Cotizaciones (producto propio) | ≈$0 (freemium) | **AVOID** (solo como módulo embebido) |
| Inventarios simples | ERP barato ya resuelto (Xubio ARS 43,6k, Odoo USD 7,25) | **AVOID** (solo implementación de Xubio/Odoo) |

**Contexto de mercado (`HECHO`, vendor studies — `WEAK` en método):** 88% de las PyMEs AR declara
intención de invertir en digitalización y 68% dice que la barrera es falta de conocimiento/personal
(Movistar Empresas); 41,6% ya usa IA pero 57% no mide impacto (1ª Encuesta Nacional AR).
**`INFERENCIA`:** el comprador es hiper-práctico, desconfiado del marketing, canal de venta WhatsApp,
operación Excel — toda oferta necesita alcance cerrado, precio publicado y respuesta por WhatsApp.

---

## PARTE 3 — NO-GO LIST

Detalle completo (riesgos, evidencia, condiciones GO) en `docs/business/project-acceptance-framework.md`.

| Trabajo | Riesgo clave | 12 meses |
|---|---|---|
| ERP completo propio | Scope infinito + 55–75% de implementaciones ERP fallan por presupuesto/plazo (Gartner vía HN) | **NO-GO** |
| Sistema contable propio | Error fiscal → sanción al cliente que te reclama | **NO-GO** (integración = INVESTIGATE) |
| Fintech | Régimen BCRA (Registro de PSP) + responsabilidad por fraude | **NO-GO** |
| Software médico regulado | ANMAT (GHEMA/HELENA/THEMIS) + IEC + vigilancia post-venta | **NO-GO** |
| POS fiscal | CAI/numeración/controladores homologados (ARCA) | **INVESTIGATE** |
| Marketplace | Cold start de dos lados + custodia de pagos | **INVESTIGATE** |
| Red social / clone | Moderación 24/7 + Ley 25.326 a escala | **NO-GO** |
| Proyecto enterprise | Procurement + net-90 + penalidades; bus factor 1 | **NO-GO** |
| Infraestructura crítica | Sin rotación (mínimo 3–4 personas para 24/7 sano) | **NO-GO** |
| Scraping dudoso | Demanda cara aunque tengas razón (Reddit v. Perplexity 2025–26) | **NO-GO** |
| Automatizaciones con APIs no oficiales | Rompen sin changelog (deprecación IG v1.0, 21-04-2025) | **NO-GO** |
| WhatsApp/Instagram no autorizados | Baneo de la cuenta del cliente (ToS Meta verificado) | **NO-GO** |
| Soporte 24/7 | No hay guardia con 2 personas | **NO-GO** |
| Proyecto enorme a precio fijo | "Fixed price projects are like handling dynamite" (HN 2026) | **NO-GO** (>2 meses de los 2 fundadores) |

**Ningún NO-GO es permanente:** todos pasan a GO con equipo ≥4–6, seguro, profesional firmante
(contador/RT), subcontratación de la parte regulada o cambio de modelo de cobro.

---

## PARTE 4 — ICP / CLIENTE IDEAL

Score de 14 verticales (dolor, pago, burocracia, ciclo, procesos manuales, repetibilidad). No vale
decir "PyMEs".

### Top 5 ICPs concretos

**ICP 1 (prioridad) — Alojamiento independiente de 5–25 habitaciones.**
Hostel de 10–40 camas / hotel boutique / cabañas, 1–4 empleados, dueño-operador en recepción.
Hoy: Booking+Airbnb, calendario en Excel o cuaderno, consultas por WhatsApp/Instagram, cobro MP.
Dolor: overbooking y no-respuesta en temporada alta (Roomy cuantifica "cada reserva perdida son
entre $50.000 y $120.000" — claim de vendor, dolor real). Disparador: temporada que se acerca +
un overbooking, o dependencia creciente de Booking. Presupuesto: SaaS de referencia USD 50–100/mes
→ **proyecto USD 1.500–3.500**. Canal: WhatsApp (el de ellos), Google Maps, ferias turísticas,
partners de Booking. Repetible: el flujo es idéntico por propiedad. Competencia MEDIA (Roomy,
AlojAR, Pxsol…) — **no competir como PMS; ganar por implementación + integración con su operación
real**, algo que un SaaS autoservicio no hace.

**ICP 2 — Inmobiliaria con 30–70 contratos.** 2–5 personas, 1 local. Hoy: 1 planilla de propiedades,
otra de clientes, visitas en WhatsApp, liquidación al propietario armada a mano con índice manual.
Dolor: liquidación mensual y ajuste de índice (errores 6–8% documentados por vendor). Ya pagan
SaaS ARS 31.800–46.900/mes (Ubiquo) → **proyecto USD 2.000–5.000 + USD 150–300/mes**. Canal:
colegios de martilleros (CIBA — ojo, Ubiquo ya tiene convenio), referidos de barrio. La parte ARCA
la resuelve su contador: **no compitas con su contador, exportá CSV para él.**

**ICP 3 — Imprenta/gráfica chica (3–15 empleados).** Cotiza 5–40 presupuestos/semana en Excel con
fórmulas repartidas. Dolor: margen perdido por cotización mal calculada (el dolor más cuantificable
de la lista). **Proyecto USD 2.000–4.500** (cotizador web + OT). Repetibilidad MEDIA: núcleo común
(cotizador + OT + clientes) con tarifas parametrizables; si es 100% a medida, no escala.

**ICP 4 — Taller mecánico 2–6 bahías.** Hoy: cuaderno + WhatsApp + planilla; "el dueño es la
conexión entre todos" (Tallerify describe el status quo exacto). Dolor: presupuesto perdido /
vehículo demorado sin avisar. Benchmark: SaaS ARS 40.000/mes → **proyecto USD 1.500–3.000**
("orden de trabajo + aviso automático por WhatsApp"). Competencia MEDIA-ALTA (7+ SaaS AR).
No prometer facturación (Tallerify dice explícitamente "no emite facturas").

**ICP 5 — Distribuidora/logística chica (5–20 empleados, 200–2.000 SKU).** Pedidos entrando por
WhatsApp sin estructura, precios por cliente en Excel viejo, stock "mirando el depósito". Dolor:
precio desactualizado y stock mentiroso. Ya pagan ARS 28.000–42.000/mes → **proyecto USD 2.500–6.000**.
Burocracia MEDIA-ALTA (remitos/ARCA → integración necesaria).

### Descartados con evidencia

- **Gastronomía:** comisiones PedidosYa 25–35% (iProfesional 2026); el restaurante chico no paga
  software, paga comisión con dolor pero no acciona. Saturado.
- **Comercio minorista de barrio:** capacidad de pago BAJA + POS baratos saturados.
- **Estudios contables:** buena plata pero saturación ALTA (Witmi, Colppy, Contabilium, Xubio…).
- **Gimnasios:** puntúan altísimo pero saturados (GymHero 850+ gimnasios, 4 SaaS nacidos 2025–26).
- **Técnicos de campo e instaladores:** menor competencia de las 14, pero evidencia 2025-26 WEAK →
  validar con 10 entrevistas antes de invertir.

**RECOMENDACIÓN:** elegir **uno** (arrancaría por alojamientos: dolor agudo y estacional → disparador
natural cada temporada) y empaquetar **un solo producto** con precio publicado, no proyectos abiertos
a medida.

---

## PARTE 5 — QUÉ SOFTWARE CONSTRUIR PARA CLIENTES

11 patrones de proceso vivo en WhatsApp + Excel + papel + memoria, con decisión por patrón:

| Patrón | Evidencia (resumen) | Decisión |
|---|---|---|
| Reservas/turnos por WhatsApp + cuaderno | Puntual, Turnos.app, Akira ("2–4h/día contestando WhatsApp") | **PRODUCTIZED** (setup de herramientas de terceros; el SaaS de agenda ya está tomado) |
| Cotizaciones a mano en Excel | CotizaGO, ObraMaestra, CotizaDigital | **TEMPLATE** (plantilla cotizadora; no SaaS propio) |
| Leads en el chat del celular | r/smallbusiness: "pipeline in a google sheet with broken vlookup" | **PRODUCTIZED** (lead pipeline / CRM setup) |
| Inventario en libreta/Excel multisheet | "inventory_FINAL_v3_USE_THIS_ONE.xlsx", cierre de 5–7 días | **TEMPLATE** por rubro; DO NOT BUILD SaaS |
| Reportes copiando de sistema a sistema | "7 Excells, 3 cuadernos y la memoria del vendedor" (rql AR) | **PRODUCTIZED** (internal dashboard) → **FUTURE SAAS** |
| Onboarding por PDF/mails/memoria | r/msp: "scaled once onboarding became templates" | **TEMPLATE** |
| Cobranzas con recordatorios manuales | "2–3 horas por semana solo mandando recordatorios" | **PRODUCTIZED** (cobranza automática; DO NOT BUILD software propio) |
| Bitácora de obra/taller en papel + grupos | LogUp, LeanCP, ObraLink (mismo diagnóstico) | **CUSTOM → FUTURE SAAS** (18–24 meses) |
| Stock con fotos y notas | "30–45 min para generar una orden de compra" | **TEMPLATE** (conteo cíclico + alerta) |
| Pedidos por WhatsApp sin carga al sistema | Automiq (AR): "cada pedido queda cargado sin doble tipeo" | **PRODUCTIZED** vertical (order desk) |
| El negocio opera sobre la memoria del dueño | "You're running on memory, not system" | **PRODUCTIZED** (diagnóstico pagado que alimenta todo) |

### De proyecto cliente a producto (sin apropiarse de nada)

Patrón publicado: Twenty One Media → *Operations Engine* (*"we kept reinventing the wrapper around
the same core work"*); Softasol (*"we kept rebuilding the same CRM patterns… so we productized
them"*); CentralCI en HN (ene-2026): se productiza el **conocimiento operativo**, no el código del
cliente; HN 43821274: *"the customer is a special snowflake but they still need 90%"* — el 90%
repetido es el producto, el 10% sigue siendo servicio facturable.

**Reglas para no apropiarse de datos/código del cliente:**
1. Cláusula de IP desde el día 1: licencia de uso al cliente + **reserva de reutilización** de
   know-how y componentes genéricos, excluyendo datos, marcas y reglas de negocio privativas.
2. Repo con *core* genérico propio y *thin layer* de cliente siempre descartable.
3. Datos del cliente **nunca** entran al módulo; demos con datasets sintéticos; consentimiento
   escrito para casos de estudio.
4. Checklist de salida de cada proyecto: ¿qué se usó igual que en otro cliente? ¿qué es 100% del
   cliente (no extraer)? ¿qué datos toqué (no guardar)? ¿qué falta para que un tercero lo entregue
   en 2 semanas sin nosotros?

**Cadencia:** 3er uso de un componente → repo de módulos; 3er entregado sin personalizar → plantilla
en venta; 5to con soporte que quema horas → candidato a SaaS (ver PARTE 13).

---

## PARTE 6 — PRODUCTIZED SERVICES

Seis ofertas cerradas, explicables y cotizables de una página. Detalle completo (alcance, NO
incluye, dependencias, costos, upsells) en `docs/business/service-catalog-2026.md`.

| # | Oferta | Resultado | Tiempo | Precio | Mantenimiento |
|---|---|---|---|---|---|
| 1 | **Operations Map** (diagnóstico pagado, puerta de entrada) | 10 fugas de workflow costeadas + plan priorizado; el cliente se queda con el documento | 7–10 días | USD 300–450 | ninguno (one-shot) |
| 2 | **Booking Setup** ("tu agenda que reserva sola") | Link de reserva + recordatorios + seña MP, llave en mano | 7 días | USD 490 setup + 89/mes | USD 89/mes |
| 3 | **Lead Pipeline** ("ningún lead se enfría") | Todo lead en pipeline con etapa, dueño y próxima acción | 10 días hábiles | USD 700–900 setup + 100/mes | USD 100/mes |
| 4 | **Quote Engine** ("cotizaciones en 3 minutos") | Cotización PDF en <3 min + seguimiento D+2/D+5 | 5 días hábiles | USD 390 setup | USD 49/mes opcional |
| 5 | **Internal Dashboard** ("el número del mes, el día 1") | Una pantalla con ventas/cobros/stock/caja, alertas por umbral | 2–3 semanas | USD 1.200–1.800 setup + 60/mes | USD 60/mes → futuro SaaS |
| 6 | **Cobranza Automática** ("recordatorios que salen solos") | Secuencia D-3/D0/D+3/D+7/D+14 + lista diaria de a quién perseguir | 7 días | USD 550 setup + 89/mes | USD 89/mes |

**Calibración:** el ticket AR aguanta USD 300–1.800 mejor que los USD 5–6k de Designjoy (que venden
a startups US); anclas locales verificadas: Expreso IA (USD 400–600 setup + 80–120/mes), landings
USD 1.500–6.000, dashboards USD 750–2.299. **Publicar el precio fijo es ventaja de venta** en una
región donde "consultar precio" es la norma.

**Orden de lanzamiento:** Operations Map (puerta barata) → Booking Setup y Cobranza (entregas de
7 días, casos de éxito rápidos) → Internal Dashboard (ticket alto).

**Decisión CUSTOM/TEMPLATE/PRODUCTIZED/FUTURE SAAS/DO NOT BUILD por patrón → PARTE 5.**
**NO construir SaaS de agenda, inventario, cotizaciones o cobranza genérico.**

---

## PARTE 7 — PRICING

### La política (7 reglas)

1. **Todo se vende por paquete con precio cerrado** (Auditoría / Implementación / Mantenimiento).
   Sin paquete = no hay propuesta. Nunca horas como unidad de venta (la hora es calculadora interna:
   si resolvés en 2h algo que ahorra 10h/semana, cobrar hora te castiga por ser eficiente).
2. **Anticipo 50%** para arrancar (50/25/25 si >USD 3k). No se empieza sin anticipo ni SOW firmado.
3. **SOW de 4 bloques obligatorios:** overview · entregables numerados · **lista explícita de
   EXCLUIDOS** · cláusula de change order (*"Todo trabajo fuera de este alcance se cotiza aparte y
   requiere aprobación escrita antes de comenzar"*).
4. **Cambio de alcance = cotización nueva.** Frase ante el "¿y si agregamos…?": *"¿Querés que te
   arme el presupuesto de eso?"*. Umbral: <2h se absorbe documentado por email; >2h = change order.
5. **2 rondas de revisión incluidas; la 3ª = change order.** Nunca "revisiones ilimitadas".
6. **Validez de cotización 15 días**; precio fijo solo para el alcance escrito.
7. **Retainer solo con entregable continuo definido + SLA + mínimo 3 meses.** Retainer sin
   definición = scope infinito a precio fijo (el peor escenario).

**Por qué importa (`HECHO`, Ignition 2025):** 57% de las agencias pierde USD 1.000–5.000/mes en
scope creep y **solo el 1% factura todo el trabajo fuera de scope**.

### Propuesta en 6 páginas

1. Problema y diagnóstico (con datos del cliente) · 2. Alcance IN en bullets contables ·
3. Alcance OUT explícito · 4. Entregables + criterio de aceptación + fechas · 5. Precio por paquete
+ pagos (50/25/25) + validez 15 días · 6. Change order + firma electrónica (las propuestas con
e-firma cierran ~4x más rápido — Proposify, `WEAK`). **Enviar siempre a 2 destinatarios**
(3+ contactos por deal = 30% close vs 14% con 1 — Optifai, `WEAK`).

### Inflación y ARS/USD (Argentina 2025–26)

- **Contexto (`HECHO`):** inflación 2025 = 31,5% (INDEC, la más baja en 8 años); REM BCRA proyecta
  ~30,5% acumulada 2026; Com. "A" 8330 eliminó el tope de USD 36.000 anuales para exportación de
  servicios; Factura E con IVA exento; PAÍS derogado; solo el 2% de los freelancers AR que trabajan
  para el exterior cobra en pesos.
- **Reglas:** (1) precio de referencia en **USD** (así cotiza el mercado local); (2) cliente en
  pesos → ARS al TC del día de la firma + validez 15 días + ajuste automático si el TC o la
  inflación mensual supera ~10% para entregas a >30 días; (3) **proyectos >30 días = hitos con
  reprecio por hito**; (4) retainer en ARS con **reajuste trimestral automático** (nunca fijo 12
  meses); (5) contrato dice "USD X o su equivalente en ARS al TC del día de vencimiento";
  (6) no vendas horas en pesos; (7) revisión de tarifario cada 6 meses o si el TC se mueve >15%.
- Sin evidencia de uso de "UF" en contratos AR → lo argentino es indexación implícita o
  dolarización de facto (`INFERENCIA`).

**[REQUIERE CONTADOR]** para facturación, retenciones, monotributo/IVA de servicios y cobranza
automatizada.

---

## PARTE 8 — VENTAS

### Pipeline completo: 10 etapas

| # | Etapa | Qué se hace | Gate de salida | SLA |
|---|---|---|---|---|
| 1 | Prospect | Carga en CRM con check ICP | Pasa ICP | Enriquecer ≤48 h |
| 2 | Research | 10–15 min: sitio, redes, stack, 1 hallazgo | Hay 1 gancho personalizado | ≤48 h |
| 3 | Contact | Primer toque (canal primario) | Sin respuesta → 2 follow-ups y archiva | ≤72 h; D+3 y D+7 |
| 4 | Qualification | ¿problema, presupuesto, decisor, timing? | Rango de presupuesto + decisor | Inbound: <1h ideal, ≤24 h máx |
| 5 | Meeting | Call 20–30 min | Agenda con fecha fija | Slot en ≤24 h |
| 6 | Diagnosis | Diagnóstico (pagado o gratis acotado) | Acta firmada / entregado | ≤5 días hábiles |
| 7 | Proposal | Paquetizada, 2 destinatarios | Enviada + fecha de decisión pactada | ≤48 h post-diagnóstico |
| 8 | Follow-up | D+2, D+7, D+14 con ángulo nuevo | Sí/no/presupuesto; 21 días sin respuesta → Lost | 3 toques máx, sin "¿algo?" |
| 9 | Close | Firma, anticipo, SOW | **Anticipo acreditado = Closed Won** | Decisión ≤14 días |
| 10 | Onboarding | Kickoff, accesos, expectativas, WhatsApp dedicado | Checklist 100% | ≤5 días hábiles desde anticipo |

### Canales: cuándo sirve cada uno

**Mezcla mínima viable (RECOMENDACIÓN):** referrals 40% + partnerships 20% + LinkedIn orgánico 20%
+ cold email señalizado 10% + auditorías/diagnósticos como oferta 10%.

| Canal | Cuándo tiene sentido |
|---|---|
| **Referrals** | **Canal #1 permanente.** Promethean: *client referrals* 3.7 y *growing current accounts* 3.6 = las únicas tácticas efectivas en todos los niveles. Heurística: ~3 referidos → 2 discovery → 1 cierre. Pedir referral **nominal** a cada cliente cerrado |
| **Alianzas** (contadores, consultores) | El contador ve las PyMEs todos los meses y decide a quién recomendar. Meta: 2–3 alianzas activas = 1–2 leads/mes c/u |
| **Diagnóstico gratuito/pagado** | Mejor puerta de entrada: scorecard 0–100 + 3 quick wins + Loom 15 min en ≤72 h. Gate: sin diagnóstico no hay propuesta |
| **WhatsApp** | Canal de relación y cierre por defecto del comercio AR (CTA dominante en sitios de agencias AR verificados). Siempre con opt-out |
| **LinkedIn local** | Con persona, no con marca; Sales Navigator solo si la lista es de 1.000–2.000 |
| **Cold email** | Solo ultra-nicho, listas <50, volumen bajo. Reply medio cayó 8,5% (2019) → **3,43% (2026)**; solo 9% de agencias lo califica "muy efectivo". **Nunca el motor principal** |
| **Contenido en español** | 1 cas/semana; Promethean puntúa blogging 1.5/5 para revenue → publicar para awareness, no para pipeline inmediato |
| **Directorios (Clutch/50Pros)** | ~10% de clientes; USD 200–500/mes; nunca pagar top spot (`OPINIÓN`/`WEAK`) |
| **Paid ads** | NO en etapa temprana (CPL USD 247–800) |
| **Gremios/cámaras** | Sin benchmark AR recuperado (`WEAK`); probar como experimento con 1 evento |

**Regla de follow-up:** 3 toques máx con contenido nuevo; en WhatsApp, una recordación con opt-out.

**NO depender de un solo canal:** el post-mortem de r/Entrepreneur `1974vor` fue literalmente cold
email como único canal; CB Insights: partnerships cayeron 44% en el año previo al cierre.

---

## PARTE 9 — CRM OPERATING SYSTEM

### Panel mínimo viable (8 métricas, revisión semanal de 20 min)

| # | Métrica | Fórmula | Benchmark / meta |
|---|---|---|---|
| 1 | Leads nuevos/semana | Prospectos que entran a etapa ≥1 | Meta inicial 8–15/semana |
| 2 | Tasa de respuesta (outbound) | respuestas ÷ enviadas | ≥5% bien, ≥10% excelente (cold 3,4% 2026) |
| 3 | Reuniones calificadas/mes | con decisor + presupuesto | Meta 4–8/mes |
| 4 | Propuesta → cierre | ganados ÷ propuestas | Mediana B2B 25%, top quartil 35% (`WEAK`) |
| 5 | Close rate global | ganados ÷ qualified | Banda agencias 25–49% (SparkToro) |
| 6 | Ticket promedio | suma ÷ deals ganados | Proyecto USD 600–3.000; setup USD 1.500–6.000 |
| 7 | Ciclo de venta (días) | cierre − primer contacto | Agencias 1–6 semanas (usar este, no el B2B genérico de 84–118) |
| 8 | Fuente | campo obligatorio por lead | 66–74% del pipeline de agencias viene de referidos |

**Desde mes 6:** MRR de retainers vs costo base · churn de retainer (meta año 1 ≤25% anual;
benchmark 18%/año retainer vs 42% proyecto, `WEAK`) · **follow-ups vencidos = 0 todos los viernes**
· velocidad de respuesta inbound <1h en horario hábil (benchmark mercado: 47h).

### Estados y disciplina

- Estados por etapa: `Active` / `Waiting` (con `next_action_date`) / `Won` / `Lost` (+ motivo:
  presupuesto, timing, competidor, no-decisión, sin respuesta).
- **Campos obligatorios para avanzar:** fuente, decisor, rango de presupuesto, fecha de siguiente
  acción. Sin esos 4, el lead no avanza.
- **Closed Won = anticipo acreditado**, no la firma. Firma sin anticipo = sigue en `Waiting`.
- `Lost-NoDecision` (22% de pérdidas post-propuesta, el motivo más recuperable) → reengagement a
  90 días con revival 18–25% (`WEAK`).

### Qué NO medir todavía

Open rate de cold email (vanity), likes/seguidores, tráfico web, MQLs y scoring, nº de emails
enviados, CAC exacto por canal (<20 deals = inválido), time-tracking fino (solo 1–2 proyectos para
calibrar precio), forecast a 6 meses (proyectar 30–45 días).
**Regla:** si la métrica no cambia una decisión de las próximas 2 semanas, no va al panel.

---

## PARTE 10 — DELIVERY

### Flujo: vendido → entregado

| # | Etapa | Entregables | Quién | Gate |
|---|---|---|---|---|
| 1 | Discovery | One-pager: problema, métrica, stakeholders, presupuesto, riesgos | Jaz (+1h técnica de Fran) | Cliente confirma "ese es el problema" por email |
| 2 | Requirements | Historias con criterio de aceptación; inventario de integraciones; lista de "no incluido" | Jaz facilita, Fran traduce | Aprobación en una respuesta consolidada |
| 3 | Scope freeze | SOW: alcance, 2 rondas, 2 hitos, exclusiones, precio fijo + tarifa de cambio | Jaz emite, Fran estima | SOW firmado + anticipo 30–50% → se abre el Change Log |
| 4 | Design | Wireframes, UI kit, copy base | Fran | 2 rondas; después no se vuelve atrás sin change order |
| 5 | Development | Repo privado, CI verde, staging siempre disponible | Fran | Historia cumple DoD |
| 6 | QA | Forms+mail, cross-browser, mobile, performance, SEO, accesibilidad, **test de restore** | Fran + Jaz (negocio) | Cero P1/P2; P3 listados y fechados |
| 7 | Client review | Ventana con **un solo mensaje consolidado** por ronda | Jaz (única boca) | Aprobación formal o lista única in-scope |
| 8 | Production | Deploy, DNS, SSL, monitoring, rollback probado, **backup restaurado una vez** | Fran | Checklist de producción (14 puntos) |
| 9 | Handoff | Credenciales en el vault **del cliente**, repo, doc, **capacitación grabada 60 min**, rotación de passwords | Jaz conduce | Acta de entrega firmada + pago final liberado |
| 10 | Warranty | Corrección de defectos contra DoD/SOW | Fran | **30 días**; email de cierre de garantía |
| 11 | Maintenance | Plan mensual con SLA y banda de horas | Jaz vende, Fran opera | Contrato activo |

### Definition of Done (12 puntos, chequeables)

1. Código en `main` con PR aprobado por el otro · 2. CI verde (tests+lint+build) · 3. Criterios de
acepción uno por uno · 4. Responsive en 360/768/1440 × Chrome/Safari/Firefox · 5. Todo formulario
envía a email real con evidencia · 6. Sin `console.log`, credenciales hardcodeadas ni TODOs en vista ·
7. Estados vacíos y de error definidos · 8. LCP <2,5s en staging · 9. SEO técnico (título, sitemap,
robots, 404) · 10. Datos de prueba en staging (no base vacía) · 11. Documento de cambio en el
changelog · 12. Jaz puede explicar qué cambió sin ayuda de Fran.

### Control de cambios (5 pasos)

1. **Canal único:** email/ticket, nunca WhatsApp "de paso". Sin ticket = no existe.
2. **Clasificación en 24h:** En-scope / Change / Postergado.
3. **Presupuesto de cambio en ≤2 días hábiles** (horas + $ + días).
4. **Tarifa:** hora suelta ×1,3 sobre la hora del proyecto, mínimo 0,5h; cambios >10% del SOW =
   nueva fase con nuevo anticipo.
5. **Change Log visible** compartido con el cliente; si el change budget (20% del proyecto) se agota,
   Fran avisa antes de seguir.

*Evidencia (r/agency 2026): "when clients see every additional request itemized, they
self-regulate"; r/agencynewbies: "si cambia el entregable es scope change, aunque tome 20 minutos".*

### Revisiones, backups, dominios, IP, docs

- **2 rondas por entregable/fase**, cada ronda = 1 mensaje consolidado; ventana del cliente 5 días
  hábiles (si no responde, se da por aprobado); rondas extra se venden en bloques.
  *"Production was rarely the bottleneck. The endless review loop was"* (r/agency).
- **Backups:** DB diaria (retención 30 días) + archivos diarios + snapshot semanal + mensual ×12;
  3-2-1; **prueba de restauración mensual registrada**. La cuenta de storage es **del cliente**
  (agencia = custodio, no dueño).
- **Dominios y credenciales:** dominio siempre **a nombre del cliente**; hosting/DB/CDN en cuentas
  del cliente con la agencia como colaborador; credenciales en el vault del cliente, jamás por
  WhatsApp/email; handoff obligatorio + rotación de passwords. Nunca retener credenciales
  ("hostage situations" — r/BusinessWebTech 2026).
- **IP:** cede el cliente → código a medida, diseños, datos, docs (una vez pagado el total);
  retiene la agencia → plantillas, componentes, librerías, starter kit, procesos, know-how
  (licenciados al cliente gratuita e indefinidamente). *"Source code ownership: absolutely
  non-negotiable — in writing before a single line of code"* (r/smallbusiness 2026).
  **[REQUIERE ABOGADO]** para la cláusula.
- **Documentación mínima antes del pago final:** README (levantar en máquina nueva) · diagrama de
  arquitectura de 1 página + inventario de integraciones · runbook (deploy, rollback, restore,
  rotación de claves) · inventario de accesos (sin secretos) · CHANGELOG + ADRs de 5 líneas ·
  capacitación grabada 60 min.

---

## PARTE 11 — EVITAR EL INFIERNO DE CUSTOM SOFTWARE

### Las 6 causas → evidencia → regla

| Causa | Evidencia | Regla preventiva |
|---|---|---|
| Stacks distintos por cliente | Ask HN 10801524: "cada cliente, alguna situación rara nueva"; "Frankenstein stack" (DoHost 2026, `WEAK`) | **Un solo stack documentado.** Desviación = **Stack Waiver** firmado por Fran con costo extra (+USD 35/mes mientras dure) |
| Tecnologías exóticas | "Choose Boring Technology" (HN 49289512, 438 pts, simonw); "Resume Driven Development" | **Prueba de reemplazo:** si un contractor AR no lo mantiene en 2 semanas con tu doc, no entra. Prohibido: lenguajes sin mercado laboral local, forks custom |
| Hosting diferente por cliente | Ask HN 10801524/16429339; shared hosting lento 2 años (r/selfhosted) | **1 proveedor, planes idénticos por perfil, panel único.** Hosting heredado solo con migración pagada |
| Customizaciones infinitas | "Scope creep hides in revision rounds" (r/agency); "while you're in there, could you just…" | SOW cerrado → Change Log → itemización → change budget 20% → "Happy to add that — it's $X and Y days" |
| Soporte sin cobrar | r/Wordpress: retainer "does not include anything they ask me to do"; "unlimited edits… not worth the headache" | Canal con horario (L–V 10–18), un solo contacto, ticket/email, SLA por plan, banda de horas. Nada de 24/7 |
| Dependencia de una sola persona | HN 49653235 (punto único de fallo), 44460428 (code review = knowledge sharing), bus factor 0 (HN 44968027) | DoD exige doc y PR; **bus factor 2 en los 5 proyectos más caros**; todo vive en cuentas del cliente; revisión cruzada mensual de 1 proyecto |

### Golden stack (sin sobre-arquitectura)

| Capa | Elección única | Por qué |
|---|---|---|
| Base de datos | **PostgreSQL** (1 motor) | Un set de runbooks, un solo restore que sabés hacer dormido |
| Framework | **1 solo: Laravel + Filament (TALL) o Node/Next** — el que ya dominan, sin volver a discutirlo | Starter kits 2026 activos en ambos ecosistemas (`HECHO`, GitHub API) |
| Hosting | **1 proveedor**, planes idénticos, panel único, backups automáticos | Mismo panel = mismo runbook |
| CI | **GitHub Actions**, 1 workflow copiado en todos los repos | Onboarding de cualquier dev en 1 día |
| Template base | **1 starter repo:** auth, CRUD, forms+email transaccional, analítica, CI, Docker Compose, script backup/restore, checklist DoD | Es lo que hace rentable el servicio (20 sitios × USD 99 ≈ USD 990/mes de margen, `OPINIÓN` de agencia) |
| Entorno | Docker Compose en 1 VPS **o** 1 PaaS (no híbrido) + staging idéntico a prod | 1 entorno = 1 memoria muscular |
| Arquitectura | Monolito modular, 1 repo por cliente, misma estructura | Menos piezas móviles |
| Observabilidad | 1 servicio de uptime + logs + alerta a 1 canal | Monitoring sin respuesta es teatro |

**Qué NO meter:** Rust/Elixir "porque queda lindo"; CMS a medida (un freelance construyó *su* CMS
en Laravel — r/webdev 1nz0ei6 = mantener dos productos con un equipo); microservicios, Kubernetes,
GraphQL sin problema de escala, colas exóticas, "features de IA" sin caso de uso medido, bases
NoSQL múltiples, builders distintos por cliente.

---

## PARTE 12 — MAINTENANCE / RETAINERS

### REAL / GRAY / ARTIFICIAL

| Ítem | Etiqueta |
|---|---|
| Hosting gestionado con parches y renovaciones | **REAL** |
| Backups **con prueba de restauración** mensual | **REAL** ("backup configurado" no lo es) |
| Updates de plugins/core con ventanas (evidencia: hack en 3 días por no actuar) | **REAL** |
| Monitoring **con respuesta** (alguien actúa) | **REAL** |
| Banda de horas para cambios chicos (2h/mes, acumulación limitada) | **REAL** |
| Reporte mensual **con lectura y 3 acciones** | GRAY → REAL (un PDF automático es GRAY) |
| SEO básico: solo auditoría automática = GRAY; cambios editoriales = REAL | **GRAY/REAL** |
| Uptime monitoring sin SLA de respuesta | **ARTIFICIAL** (cuesta USD 2/mes) |
| "SSL" / "copias" solo administradas por el proveedor | **ARTIFICIAL** (reventa declarada) |
| "Soporte 24/7" sin banda ni límite | **ARTIFICIAL y caro** (se vende algo incumplible) |

**Regla de oro:** si en el informe mensual no podés escribir *"esto hicimos, esto evitó, esto
queda"*, el ítem es GRAY o ARTIFICIAL. El mercado ya lo reclama: *"I'd like to see
deliverables"* (cliente pagando USD 950/mes, r/smallbusiness 2025).

### Planes sugeridos

| Plan | Contenido | Precio |
|---|---|---|
| Hosting+ | Solo hosting con respaldo y uptime — **venderlo honesto** | USD 15–25/mes (GRAY/ARTIFICIAL) |
| **Care** | Updates, backups con restore probado, monitoring con respuesta, 1h de cambios | USD 49–79/mes (≈ ARS 50–80k) |
| **Care+** | Care + 2h de cambios + informe con 3 acciones + SLA en horario hábil | USD 99–149/mes |
| Business/SLA | + SLA de respuesta, staging, auditoría de seguridad, prioridad | USD 199–299/mes |

*Anclas verificadas: geary "USD 99/mo is the absolute minimum"; buildwithrab: a USD 79 el margen es
~30% y "two unbilled support emails erase it"; mercado AR: mantenimiento web ARS 49.900–130.000/mes.*

### Si el cliente "solo quiere hosting"

1. Hosting-only **permitido** con alcance escrito: sin updates, sin SLA, backups del proveedor
   **sin prueba de restore**. Precio bajo, expectations claras.
2. La **garantía de 30 días no se pierde**; el SLA y la banda de horas sí. Si se rompe por no
   actualizar → se factura como incidente (tarifa mínima de emergencia avisada desde el día 1).
3. Costo de salida predefinido y razonable (USD 0–50 por exportar backup + transferir accesos en
   ≤5 días hábiles). **Nunca retener credenciales.**
4. Objetivo de mezcla: ~20 sitios × USD 99 con golden stack. Donde solo quieren hosting, la jugada
   es bajar el costo de servirlos y dejar la puerta abierta al plan Care.

---

## PARTE 13 — SOFTWARE FACTORY (agencia → producto)

### Criterios objetivos (cumplir MAYÚSCULAS antes de construir)

| # | Criterio | Umbral |
|---|---|---|
| 1 | Clientes distintos con el mismo problema raíz | **≥ 3–5 no relacionados** |
| 2 | Contenido repetido entre proyectos | **> 40%** del entregable |
| 3 | Segmento homogéneo con canal nombrable | Si no nombrás el canal, no hay distribución |
| 4 | Uso recurrente, no proyecto único | El problema vuelve mensual/semanal |
| 5 | Presupuesto demostrado | **≥ 2 clientes ya pagaron** una versión semi-repetida |
| 6 | El a medida ya no aprende | La 4ª repetición no aporta conocimiento nuevo |

**Regla dura:** 1 solo cliente pidiendo el producto = encargo con factura de SaaS. **No construir
SaaS por petición de un único cliente.** Patrón publicado: 45Squared (*"el trabajo de cliente y el
producto compiten por las mismas horas… sin cambiar la asignación de horas, matás el producto"*),
Softasol, CentralCI, Twenty One Media.

### Cómo no apropiarse de datos ni código

1. **Reescribir, no extraer** (test: ¿podrías defender clean-room respecto a la spec del cliente?).
2. **Datos del cliente = fuera.** Paradigma Timescanner: *"your calendar is the database, we don't
   store your events"* — la arquitectura privada por diseño elimina la tentación.
3. Cláusula contractual desde hoy: IP del motor genérico (escrito sin copiar del repo del cliente)
   vs IP del caso específico.
4. NDA: podés usar el **conocimiento** (qué problema existe), no el **material**.
5. Validar con el cliente cuando el producto compite en su mercado — a veces es tu primer canal.

---

## PARTE 14 — PORTFOLIO FD LABS

### Cuántas horas existen realmente

**`HECHO` (SPI Research 2025/2026):** utilización billable promedio **66,4% — mínimo histórico**
(óptimo teórico 75%). Para 2 personas: 2 × 40h × 66–75% = 52–60h/semana facturables (la agencia);
restando admin/ventas/soporte → **~10–14h/semana reales para productos, ambas personas combinadas.**

| Iniciativa | Horas/semana |
|---|---|
| **PULSAR (flagship)** | 5–6 h |
| Open source (soporte/triage) | 1–2 h |
| Producto pequeño | 2–3 h |
| SaaS en validación (casi todo entrevistas/ventas) | 2 h |

**Si la agencia entra en pico >75% de utilización, los productos se congelan — no al revés.**

### Reglas de añadir / parar

- **Añadir solo si** (a) uno de los 4 está en maintenance mode (≤1h/semana), o (b) el nuevo nace
  *dentro* de uno existente (componente extraído de PULSAR). **Nunca un 5º palo paralelo.**
- **Parar a los 90 días** si cumple 2 de: 0 clientes pagos · sin uso semanal creciente · consume
  >50% del presupuesto de producto sin señal de tracción.
- **Congelar PULSAR primero es error:** es el activo de reputación que alimenta todo lo demás.
- **WIP:** máximo 4 initiatives activas, solo 1 "grande".

---

## PARTE 15 — OPEN SOURCE STRATEGY

### Qué se gana (sin asumir monetización directa)

Reputación/portfolio público (el repo es el CV del estudio) · recruiting filtrado (C++20/Qt6/FFmpeg
= el filtro duro que querés) · confianza comercial en la venta ("si open-sourcean el motor, ¿qué
esconden?") · componentes reutilizables (el core de PULSAR ya es fuente de componentes) · issues =
research gratuito de problemas parecidos a los de tus clientes.

### Qué abrir / qué no

| **Abrir** | **No abrir** |
|---|---|
| Core de PULSAR (ya OSS) | Código específico de clientes (contractual + NDA) |
| Componentes genéricos extraídos (parsers, UI kits, pipelines) | Datos/telemetría/secretos de clientes |
| Herramientas "boring but useful" (CI, lint, migradores) | Diferenciadores 1:1 con tu oferta de pago que se copian trivial |
| Docs, benchmarks, datasets sintéticos | Lo que genera soporte masivo sin retorno |

**Test:** ¿abrirlo me cuesta horas de soporte o me da horas de ventas? Si es lo primero sin retorno:
"supported, not maintained" con política explícita, o no abrir.

### Licencias

- **MIT / Apache-2.0 por defecto** (adopción + recruiting + confianza; Apache añade patentes y es
  CLA-friendly → prepara dual-license si algún día hace falta).
- **AGPL/GPL solo** si querés que las mejoras vuelvan (SaaS full-stack propio); reduce adoptadores
  comerciales.
- **No relicenciar después:** ahí la comunidad te deja de creer (Cal.com → closed source, HN abr-2026;
  InfluxDB muestra que permissivo + producto de pago funciona sin AGPL).

### Governance y carga real

- **Presupuesto duro ≤ 2h/semana** de OSS (triage + bugs críticos). El piso documentado de triage
  en repos populares es **~2,5h/semana** (media 127 issues abiertos, 34% sin respuesta >7 días).
  Si supera 3 semanas seguidas → congelar features nuevas.
- **Tiered support:** "critical bugs only" para la comunidad; SLA solo para clientes pagos
  (puente OSS → servicio).
- **18 meses sin features de terceros a voluntad** — aceptar fixes y docs. BDFL (dueño de PULSAR) +
  CONTRIBUTING.md claro + CLA simple si Apache.
- Decidir **por escrito** qué es "community-supported" vs "FD-maintained" desde el día 1: el burnout
  viene del scope ambiguo, no del volumen (SonarSource: 58% dejó o consideró dejar; Tidelift 2024:
  ~60% no recibe remuneración y 44% reporta burnout).

### Financiamiento (sin asumir que OSS monetiza directo)

**No contar con donaciones:** Directus (15k stars, 4 FTE) recaudaba ~USD 1.000/mes (HN feb-2025);
tw93 (134k+ stars) tiene 14 sponsors activos. Modelos que sí: **soporte e implementación vía
Solutions Tech** · hosting/integración alrededor del producto · open core sin paywallar lo que ya
era gratuito · training. Open Source Pledge (~USD 2.000/dev/año) es pago por uso, no donación.

---

## PARTE 16 — SAAS STRATEGY

### Checklist mínimo de lanzamiento (todos, verificables antes de codificar producción)

1. **Dolor recurrente** — el problema aparece ≥ mensualmente en 3–5 clientes del patrón.
2. **Uso recurrente** — si no vuelve semanal/mensualmente, no es SaaS: es one-off con suscripción falsa.
3. **Voluntad de pago demostrada** — **≥ 1 pago real antes de escribir código de producción**
   (Freemius 2025: trial opt-out convierte ~50% vs opt-in 18% vs freemium 3–4%; ~70% de founders
   de micro-SaaS piden tarjeta desde el día 1).
4. **Mercado accesible** — podés nombrar el canal.
5. **Distribución propia** — comunidad OSS, lista, SEO o la base de Solutions Tech (47% de founders
   dice que integraciones/comunidades rinden más que anuncios; 57% que anuncia no ve ROI ni a 7 meses).
6. **Mantenimiento soportable** — ≤2h/semana de soporte en estado estacionario.
7. **Umbral honesto** — mediana de micro-SaaS rentable ≈ **USD 4,2K MRR**; ~70% queda debajo de
   USD 1K MRR. Con <USD 1K no se paga una persona en AR → medir también contra valor estratégico
   (datos, referencia, canal).

### Qué SaaS evitar

**1. Adquisición que cuesta más que el producto.** Benchmarks 2025 (`WEAK`, orden de magnitud):
CAC mediana USD 702 (SMB USD 450), payback mediano 17,3 meses, LTV:CAC 3,8:1. Si tu precio es
<USD 50/mes y necesitás pagar >USD 700 por cliente → **no lanzar con adquisición pagada.**

**2. Soporte permanente.** Precios B2B suben 8–12%/año y el costo de atender sube aunque tu costo
técnico no. Con 2 personas, soporte ilimitado duplica tu problema de utilización. → tiers, no
"ilimitado" en el plan base.

**3. Una API externa controla tu negocio** (`HECHO`, todos verificados en HN):
- **Bing Search APIs** — retiro anunciado 11-ago-2025 (HN 44050200): productos enteros a migrar o morir.
- **DeepSeek** — aviso de subida "significant" de precios de API (ago-2026, HN 49196356): hasta el
  proveedor "barato" reprecia.
- **X Video API ("Vid")** — shut down tras un correo (feb-2025, HN 43034994).
- Postmortems en r/SaaS: "We shut down our SaaS after 18 months", "We sunset our API" (2025–26).

**Mitigación:** clause review de cada API antes de construir (¿qué pasa si desaparece mañana? ¿cambio
en <2 semanas?); nunca único proveedor en el path crítico sin fallback/self-host; **reservar
1 sprint/año** para reemplazar el proveedor más riesgoso aunque no falle; soporte acotado por tiers.

---

## PARTE 17 — DISTRIBUTION

### Ruta a 10 / 50 / 100 clientes por motor

**Motor agencia:**

| Hito | Cuándo | Motor dominante | Acción concreta |
|---|---|---|---|
| **10 clientes** | 0–6 meses | Referrals + red + 1–2 alianzas | Decirle a 50 contactos exactamente qué vendés y a quién · pedir referral nominal a los primeros 3–5 clientes · cerrar 1 alianza con estudio contable · 10 diagnósticos gratis/mes |
| **50 clientes** | 6–14 meses | Sistema: referral + upsell + contenido + outbound nicho | Cada proyecto cierra con mantenimiento ofrecido · 2–3 alianzas activas (1–2 leads/mes c/u) · contenido semanal en español · outbound nicho 30–50 contactos/mes. **Ya requiere subcontratar delivery parcial** |
| **100 clientes** | 14–30 meses | Deja de ser "agencia de proyectos" | Solo alcanzable si la mayoría son **mantenimiento/producto estandarizado** (1–3h/mes) + 1–2 externos de delivery. 100 × 2h/mes = 200h/mes → inviable con 2 sin estandarizar |

**Motor open source:** Show HN (solo si se puede probar) · directorios (Awesome Launch, OpenAlternative
— lanzó con 100K visitantes y #1 en HN, AlternativeTo, LibHunt) · comunidades Linux/español/DEV/
Lobsters/Stacker News · micro-newsletters de 5.000 suscriptores > audiencias de 100.000 genéricas.
Hitos: 10 = conversaciones directas + 1 Show HN + 3–5 directorios → 50 = comunidad + newsletters →
100 = SEO del repo/docs + 1 comunidad sostenida.

**Motor desktop:** AlternativeTo, "open source alternative to X", OpenAlternative, Flathub/Homebrew,
GitHub Topics, Product Hunt, DevHunt, r/selfhosted, r/linux, newsletters FOSS. Evidencia de demanda:
OpenCut (alternativa OSS a CapCut) → **88.820 stars** desde jun-2025. Evidencia de monetización
débil: los desktop OSS populares monetizan por sponsorship marginal → **tratarlo como generador de
audiencia/credibilidad, no de revenue**; vía realista = core libre + builds Pro de pago.

**Motor SaaS:** 5 canales gratis 2025 (build in public en X, Indie Hackers, Reddit con valor previo,
newsletters/directorios, free tools) · roadmap 0→1.000 usuarios: 100 (DMs de 50 prospects) → 500
(Reddit semanal + Product Hunt + 5 guías SEO) → 1.000 (doblar canales ganadores) con **~90 min/día**
de growth · SEO desde el día 1 con keywords de nicho (6–12 meses a tráfico) · "las integraciones son
ahora canal de distribución, no feature". **No escala temprano:** paid ads, webinars, podcasts.

**Qué NO escala:** founder-comercial como único cierre (techo 2–4 cierres/mes) · proyectos 100% a
medida · LinkedIn frío genérico (reply 5–10% y cayendo) · esperar SEO en <6 meses · **contratar
vendedor antes de tener posicionamiento y proceso** ("un vendedor no puede vender un positioning que
no existe").

---

## PARTE 18 — BRAND POSITIONING

### Alternativas (elegir y sostener, no mezclar)

Marco verificado (Searchlab, "Marketing for a Two-Person B2B Agency", 2026): nicho = **vertical ×
servicio × etapa/tamaño**, que deja 500–5.000 empresas objetivo: *"Niche, every time… your
positioning is what wins inbound, not what restricts your invoicing."*

1. **"Menos planillas, un sistema"** — operaciones digitalizadas para PyMEs que ya venden. Riesgo:
   suena a consultoría genérica sin rubro nominado → **nombrar rubro en el titular**.
2. **"Tu operación en el bolsillo"** — apps internas/flujos a medida para equipos de 5–50, piloto de
   3–4 semanas. Riesgo: colapsa en "hacemos apps" si no se comunica el resultado operativo.
3. **"Infraestructura y soporte como servicio"** — continuidad técnica mensual. Riesgo: commodity
   contra informática local, techo de ticket bajo.
4. **"El mismo problema, tercera vez → producto"** — producto empaquetado desde servicios
   (crece con la lista de PARTE 5). Riesgo: el de überdosis — mantenimiento del producto compitiendo
   con deadlines de clientes.
5. **"Open source primero, dinero después"** — utilidad pública + soporte/implementación pagas.
   Riesgo: demanda de mantenimiento sin estrategia clara.
6. **"Vertical X con operación medible"** — rubro concreto × un servicio × una etapa, con número y
   precio en el titular. Ejemplos verificados: Animalz (*"for Leading B2B SaaS Brands"*), WyeWorks
   (stack acotado), MakingSense (AR/US, métricas duras: *"60% reduction in manual processing time"*).

### RECOMENDACIÓN

- **Elegir 6 + 1:** *"Encontrá y perdés reservas por WhatsApp — te dejamos la operación de [alojamientos
  de 5–25 habitaciones] en un sistema, en 2 semanas y con precio cerrado"* (o la vertical que se
  elija de PARTE 4). Con caso publicado y precio publicado.
- Evitar los tres vetados: "hacemos páginas web", "somos expertos en IA" (saturado — MakingSense ya
  lo usa *con datos*; sin dato es ruido), "transformamos digitalmente tu empresa".
- Trabajo fuera de nicho: *"Take the off-niche work when it shows up, and don't update the homepage
  to chase more of it."* (Searchlab).

---

## PARTE 19 — ROLES

### Responsabilidades exclusivas

**Jaz (demand side):** posicionamiento y mensajes · contenido y casos · dominio del CRM y prospección
activa · reuniones, propuestas, precios de venta · seguimiento y relación con clientes · cobros ·
red y alianzas.
**Fran (supply side):** estimación técnica y promesa de alcance · arquitectura, desarrollo,
implementación · infraestructura, seguridad, backups · soporte y SLA · calidad/estimación del
producto · roadmap técnico de FD Labs · evaluación técnica de futuros hires.

### Decisiones compartidas (con desempate)

Precios (Jaz propone mercado, Fran valida horas/riesgo) · qué proyecto aceptar · backlog del
producto · contratación · marcas/dominios · gastos > umbral acordado · "no" a un cliente.
**Desempate: veto en el propio dominio** (Jaz decide mensaje/segmento; Fran decide viabilidad
técnica) y, si hay empate material, decisión alternada por trimestre.

### Qué NO hacer

- **Jaz:** no prometer fechas ni arquitectura · no "arreglar" producción · no gestionar soporte
  técnico (destruye la agenda comercial). El tono y la oferta son "founder-articulated": no se
  delega marketing hasta tener oferta clara.
- **Fran:** no vender como vendedor ni decir "hacemos todo" · no disputar el mensaje público con el
  cliente adentro de la sala · no ser soporte 24/7 personal (se vuelve cuello de botella de todo).
- **Ninguno:** cargar con delivery + ventas + soporte a la vez (*"agency life is probably 80% sales
  and 20% delivery"* — HN 2025).

### Cuándo contratar y a quién

Umbral (r/agency, `FatMarketer123`): ambos socios trabajando 60+ h/semana **y** flujo de caja
estable. Entonces:
1. **Primer hire = operaciones de entrega / account ops** (part-time o prueba 90 días, **no
   junior**): reportes, coordinación, soporte L1, mantenimiento del CRM. Libera horas de Jaz y Fran.
   *"Your first hire should not be a junior."*
2. **Segundo = dev senior o QA/infra** solo si Fran es el cuello de botella del producto.
3. **No contratar "marketing"** antes de que Jaz tenga mensaje y caso probados.
4. Preferir contractor antes que full-time si el ingreso no es estable.

---

## PARTE 20 — CAPACIDAD

### Límites duros (equipo de 2)

Supuestos: 44h nominales/persona · Jaz 12h ventas + 8h reuniones + 24h entrega · Fran 4h ventas +
6h soporte + 34h entrega (6h a producto) → **~44h productivas de proyecto** (menos 15% de fricción
por context-switching; overhead por cliente 2–4h/semana).

| Límite | Valor | Motivo |
|---|---|---|
| **Proyectos activos (build)** | **Máx 3** (o 2 si alguno es custom/integrado) | 44h ÷ ~13h por proyecto; el 4° se come soporte o sueño |
| **Clientes de mantenimiento** | **Máx 15–20** | 15 × 2–3h/mes ≈ 8–11h/semana; más, y el soporte contamina los builds |
| **Pipeline de Jaz** | 40–60 contactos nuevos/mes y 8–12 reuniones/mes | 12h/sem de BD; cierre esperado 2–3/mes |
| **Horas de producto** | 6h/semana conjuntas (p. ej. "FD Friday" quincenal) | Primero que se recorta si no está hardcodeado |
| **Reuniones por proyecto** | ≤2/semana, 30 min | Overhead documentado |
| **Respuesta de soporte** | 48h (no 4h) | Mantenimiento no es emergencia sin plan SLA |

*Referencia externa: Soglia Ventures (studio de 2) = 4–6 clientes/año + medio año en productos;
2D2C rechaza 1 de cada 3 briefs para no sobrecargar el calendario; freelancer full-time maneja
2–4 proyectos (30–35h billables + 6h admin).*

### Regla anti-sobreventa

> **No se firma nada si el tablero ya muestra 3 WIP o si la semana siguiente no tiene 10h libres
> reservadas para el nuevo proyecto.**

1. **Regla del turno:** con 3 activos, el próximo cierre tiene fecha de inicio "cuando cierre el
   proyecto A" — y eso se dice **antes** de firmar.
2. **Regla del 70%:** nunca >70% de las horas semanales asignadas a proyectos; 30% = ventas +
   soporte + admin + imprevistos.
3. **Regla del presupuesto de horas:** si el acumulado de las próximas 4 semanas > 44h × 4 × 0,7,
   no se acepta.
4. **Regla del "no a tiempo":** los rechazos se hacen ofreciendo fecha futura, no precio menor.
5. **Trigger de subcontratación:** 2 meses seguidos facturando <80% de la capacidad por falta de
   delivery → freelance de delivery (no empleado).

*No confundir techo de demanda con techo de entrega (Haus Advisors): delivery ceiling = utilización
alta, espera larga, rechazás work; demand ceiling = founder haciendo ventas "entre otras cosas".*

---

## PARTE 21 — ECONOMÍA

### Supuestos de precio (mercado AR verificado, 2026)

| Ítem | Rango |
|---|---|
| Landing | ARS 850k–1,5M / USD 300–700 |
| Sitio PyME 3–8 págs | ARS 1,2M–2,5M |
| E-commerce | desde ARS 3M |
| Mantenimiento web | ARS 25k–150k/mes (profesional 80–150k) |
| Hosting cliente | ARS 5k–40k/mes |
| Monotributo servicios (sep-2026) | Cat. A 49.527 · C 66.020 · E 119.811 (tope $36,0M) · H 522.706 |
| Referencia salarial | Backend junior ARS 2,35M/mes · senior ARS 5,61M (Salancy 1/1/26) |
| Software de agencia | USD 150–300/mes (lean) |

**Precio unitario usado en los escenarios (`INFERENCIA`):** proyecto web PyME = ARS 1.500.000 ·
mantenimiento = ARS 90.000/mes · landing = ARS 850.000.

### Escenarios mensuales — Solutions Tech

| Concepto | 🔴 LOW | 🟡 EXPECTED | 🟢 STRONG |
|---|---|---|---|
| Proyectos | 1 landing (850k) | 1 web PyME (1,5M) | 2 webs (3,0M) |
| Mantenimientos | 5 × 90k = 450k | 8 × 90k = 720k | 12 × 90k = 1,08M |
| Otros (auditorías) | 0 | 150k | 300k |
| **INGRESO BRUTO** | **ARS 1,3M ≈ USD 870** | **ARS 2,37M ≈ USD 1.580** | **ARS 4,38M ≈ USD 2.920** |
| % recurrente | 35% | 36% | 31% |

### Costos mensuales (categorías)

| Categoría | 🔴 LOW | 🟡 EXPECTED | 🟢 STRONG |
|---|---|---|---|
| **Herramientas/SaaS** | 225k | 375k | 600k |
| Hosting + dominios | 60k | 120k | 200k |
| Contador | 100k | 150k | 250k |
| Impuestos/cargas (2 monotributos) | 186k | 186k | 420k (recategorización) |
| Comisiones de cobro | 40k | 80k | 150k |
| Marketing | 50k | 200k | 400k |
| Suscripciones editoriales | 30k | 60k | 100k |
| Hardware (amortizado) | 150k | 150k | 250k |
| Subcontratación de delivery | 0 | 300k | 900k |
| **TOTAL COSTOS** | **ARS 891k ≈ USD 595** | **ARS 1,62M ≈ USD 1.080** | **ARS 3,27M ≈ USD 2.180** |
| **NETO (antes de sueldos)** | **ARS 409k ≈ USD 270** | **ARS 749k ≈ USD 500** | **ARS 1,11M ≈ USD 740** |
| Neto ÷ 2 personas | USD 135 | USD 250 | USD 370 |

> **Lectura honesta (`INFERENCIA`):** LOW **no paga dos salarios** (un backend junior AR cuesta
> ARS 2,35M/mes). **Piso de supervivencia ≈ ARS 3.000.000/mes brutos** = 1 web PyME + 10
> mantenimientos + otros. Sin proyectos, harían falta 33 mantenimientos → **inviable con 2.**

**Umbral impositivo a prever (`HECHO` ARCA):** al pasar de cat. E (tope ~ARS 3,0M/mes) a G la cuota
salta de 119.811 a 230.312 — prever en el pricing.

**Fórmula del mes LOW:** `N_proy × 1,5M + N_mant × 90k ≥ 891k` → 1 proyecto **o** 10
mantenimientos **o** 1 landing + 5 mantenimientos.

### FD Labs — escenarios

| Escenario | Ingresos | Costos |
|---|---|---|
| 🔴 LOW (0–12 meses) | **USD 0** | USD 30–80/mes (opción de mercado) |
| 🟡 EXPECTED (12–24m, OSS con tracción + 1 app de pago) | USD 0–200/mes | USD 50–150/mes |
| 🟢 STRONG (24–36m, SaaS con 30–100 pagos × USD 5–15) | USD 150–1.500 MRR | USD 100–300/mes |

**Regla de asignación:** FD Labs solo consume las 6h/semana ya reservadas **y** solo si Solutions
Tech está en ≥EXPECTED. Si Solutions Tech cae a LOW → FD Labs = 0h esa semana.

---

## PARTE 22 — LEGAL / RISK (operativo — no asesoría jurídica)

| Tema | Riesgo operativo | Medida práctica | ¿Quién valida? |
|---|---|---|---|
| **Contratos** | "Terminado" según el cliente ≠ según vos; pagos a 60 días; sin límite de responsabilidad | MSA corto + SOW por proyecto: alcance con "done = criterio X", 50% arranque, garantía 30 días solo bugs reproducibles, responsabilidad limitada a lo facturado en 12 meses, terminación 15 días con pago de ejecutado, cambios solo por escrito | **[ABOGADO]** — que las limitaciones sean exigibles |
| **Propiedad intelectual** | Sin cesión escrita, el código es del desarrollador; disputa por "todo el código" | Cede: código a medida pagado. Retiene: componentes/genéricos/know-how con licencia perpetua al cliente. Anexo de componentes preexistentes | **[ABOGADO]** |
| **Licencias OSS/GPL** | AGPL dentro del SaaS propio; binario con GPL | Preferir MIT/Apache/BSD/MPL; SBOM mínimo (`license` por paquete) por entrega; nada de AGPL en el CRM/servicio SaaS | **[ABOGADO de software]** solo casos AGPL/binarios |
| **Datos personales — Ley 25.326** | Sanciones (Arts. 31–32, CP); el CRM propio podría estar "destinado a dar informes" (Art. 21 → Registro AAIP); Art. 25: destrucción/devolución de datos tratados por cuenta de terceros al terminar, retención máx. 2 años; Arts. 14/16 plazos de respuesta (5/10 días); Art. 12 + Disp. 60/2016 si hosting/CRM/email en el exterior | Política de privacidad visible (Art. 6) + baja en cada email (Art. 27) + cifrado/acceso restringido (Art. 9) + registro de accesos + retención definida + cláusula de tratamiento con cada cliente (quién es responsable, qué pasa al terminar) | **[ABOGADO]** — criterio de inscripción del CRM propio es el punto más gris |
| **Credenciales y cuentas** | Disputas reales: agencias reteniendo dominios/Google Business como primary owner | Dominio/hosting/ads/repo **en cuentas del cliente desde el día 1**; agencia = colaborador; vault por cliente; handoff con rotación de passwords; cláusula de transferencia en 5 días si hubo cuenta puente | Prevención propia; **[ABOGADO]** solo en disputa activa |
| **Backups** | No hacer backups puede costar el derecho a reclamar (UNCITRAL); Art. 9 obliga a evitar pérdida | Backup automatizado + **prueba de restore trimestral/mensual**; responsabilidad escrita; servicio de backup facturado con RPO/RTO si se ofrece | **[ABOGADO]** cláusula de exclusión |
| **Garantía vs mantenimiento** | Confundirlos = soporte gratis | Garantía 30 días (defectos) incluida; desde el día 31, soporte solo con retainer (horas, canal, tiempos, exclusiones); acta de puesta en producción firmada | **[ABOGADO]** si hay SLA con penalización |
| **Facturación, retenciones, monotributo, cobranza automática** | Errores impositivos | No automatizar montos ni impuestos sin criterio | **[CONTADOR]** |
| Colaboradores/subcontratistas | Encadenar responsabilidades de IP si no firman cesión | Que todo contractor firme cesión/NDA | **[ABOGADO]** |

Texto oficial Ley 25.326: argentina.gob.ar/normativa/nacional/64790/actualizacion · obligaciones
AAIP: argentina.gob.ar/aaip/datospersonales/responsables/obligaciones.

---

## PARTE 23 — AUTOMATIZACIÓN (de la propia agencia)

**Regla de decisión:** automatizar solo si
`Frecuencia × Tiempo ahorrado × Tasa de error > (horas de construir + horas/mes de mantener)`.
Umbral: caso **<4 veces por mes** y **<15 min** cada vez → es plantilla o checklist, no software.
Segundo filtro: **¿el error humano de este paso cuesta plata o reputación?** (si sí, automatizar
aunque ahorre poco). Tercero: **¿ya lo hace la herramienta actual?**

| Caso | ¿Vale la pena? | Cómo |
|---|---|---|
| Prospect research | **Sí** (10–20 leads/semana) | Plantilla de prompt + LLM → webhook al CRM (n8n/Make). No un "sistema de research" propio |
| CRM: sync y recordatorios | **Sí si ya lo hace el CRM** | El propio CRM primero; Make (flujos simples) o n8n self-hosted (control y costo fijo) |
| Propuestas | **Sí** (5–10/semana, 30–60 min c/u) | Plantilla + snippets/LLM para borrador; no un generador propio |
| Onboarding de cliente | **Sí, parcialmente** | Checklist + secuencia de emails en el CRM. No un portal propio |
| QA checks | **Sí, alto ROI** | GitHub Actions: lint, link check, Lighthouse CI, screenshot diff, forms |
| Deployments | **Sí, ya existe** | GitHub Actions. No inventar pipeline propio |
| Reporting a clientes | **Sí si es mensual para varios** | Looker Studio/Grafana o script de export; no una app propia |
| Follow-ups de prospección | **Sí** — ahí se pierden deals | Secuencias del CRM |
| Billing reminders | **Sí, mínimo esfuerzo** | D-7/D-0/D+7 desde CRM/planilla. Cobranza legal = contador |
| Sincronizar 2 herramientas raras | A veces | n8n/Make o script de 50 líneas con cron |
| Tareas que ocurren 1 vez al mes | **No** | Plantilla + 20 min manuales |
| Portal de cliente, chatbot interno, tickets propios | **No** | Ver PARTE 24 |

---

## PARTE 24 — SOFTWARE INTERNO

**Sí vale la pena (ROI claro, chico):**
1. **Repo de plantillas** (props, onboarding, checklists, snippets) en GitHub.
2. **CI/QA mínimo** reutilizable en proyectos de clientes.
3. **Script de billing reminders + reporte mensual** desde el CRM/planilla.
4. **Dashboard de pipeline** con lo que ya tenés (CRM o Looker Studio sobre export).
5. **Vault de credenciales + inventario de accesos por cliente** (herramienta existente).

**NO construir internamente:** ERP propio · sistema de tickets propio · **un CRM adicional** al que
ya tenés · planilla de timesheets hecha app · portal de cliente con login propio · sistema de billing
propio · "plataforma de onboarding" propia · editor de propuestas propio · base de conocimiento
propia (GitHub/wiki/Notion existen) · automatización casera de cobranza con montos/impuestos.

**Regla:** si una herramienta existente lo hace al 80%, se compra o se usa; solo se construye si es
vuestra **diferenciación vendible**. *"Spreadsheets are the internal MVP that proves business value
before you build the centralized systems"* (HN). **No crear un ERP de FD Labs.** A 2 personas, la
hoja de cálculo es la herramienta correcta (r/lowcode).

---

## PARTE 25 — 90-DAY PLAN

**Pocas prioridades: (1) cerrar la oferta, (2) llenar el pipeline, (3) entregar con proceso, (4)
medir lo mínimo.**

### Días 1–30 — Fundación
- **Oferta:** 3 paquetes cerrados con precio publicado (Operations Map USD 300–450 · 1 paquete de
  entrada de sitio/landing · 1 paquete de automatización/migración) + exclusiones + change order.
- **Proceso de venta:** SOW, propuesta de 6 páginas, checklist de DoD, formulario de change log
  escritos y probados internamente.
- **ICP:** elegir 1 vertical principal (recomendación: alojamientos) + 1 secundaria de test
  (inmobiliarias); escribir el pitch de una línea por rubro.
- **Prospecting:** lista de 50 contactos de red · pedir referral nominal a los primeros clientes ·
  1 reunión con estudio contable/consultor para alianza · diagnóstico gratuito publicado (web +
  WhatsApp).
- **CRM:** etapas de las 10 fuentes + 4 campos obligatorios + `Closed Won = anticipo acreditado` +
  panel de 8 métricas.
- **Portfolio:** diagnóstico completo del sitio/casos; si no hay caso publicable, cobrar el primer
  proyecto como caso (consentimiento escrito).

### Días 31–60 — Primeras ventas con el nuevo sistema
- 10 diagnósticos ("¿dónde perdés horas por semana?") por WhatsApp a la vertical elegida.
- Cerrar 2–3 proyectos con pricing nuevo (paquete + anticipo 50% + 2 rondas); adjuntar oferta de
  mantenimiento a **todo** cierre.
- Arrancar el **starter repo** (golden stack) y correr el primer proyecto sobre él.
- Delivery en modo checklist: DoD, change log visible, handoff con acta y capacitación grabada.
- Contenido: 1 caso o aprendizaje semanal en español.
- **10 entrevistas de cliente/prospect** (ver UNKNOWNS abajo).

### Días 61–90 — Medir y ajustar
- Revisar panel: leads/semana, reuniones, propuestas, close rate, ticket, ciclo, fuente → recortar
  el canal que no rinde, duplicar el que sí.
- **Test de precio:** subir 20% en las próximas 10 propuestas (o bajar si close rate >60% = estás
  barato).
- Segunda ronda de alianzas (2–3 activas).
- Scan de "software factory": listar qué se repitió en los proyectos del trimestre (criterios de
  PARTE 13) — **sin construir producto aún**, solo evidencia.
- FD Labs solo en las horas reservadas y solo si la agencia está en ≥EXPECTED.
- Revisar el pre-mortem (PARTE 27): ¿alguna early warning encendida?

**Métricas del día 90:** pipeline ≥3 meses de ingreso · ≥2 mantenimientos activos · 1 canal
funcionando con ≥40% de los leads · pricing validado (close rate 20–40%) · 0 follow-ups vencidos.

---

## PARTE 26 — 12-MONTH ROADMAP

### Fase 0–3 meses: "Cerrá el sistema" (DONE = los 90 días de arriba)
**Gates para pasar a la Fase 3–6:**
- ≥10 clientes cerrados con paquetes (no proyectos abiertos);
- ≥2 mantenimientos activos y cobrados;
- proceso de delivery corriendo con DoD + change log en ≥2 proyectos;
- panel de 8 métricas con 4+ semanas de datos;
- pipeline con ≥3 meses de ingreso proyectado.

### Fase 3–6 meses: "Sistematizá"
- 2–4 cierres/mes sostenidos; 5–10 mantenimientos; **primer trigger de subcontratación** evaluado
  (2 meses <80% de capacidad por falta de delivery → 1 freelance).
- 2–3 alianzas activas trayendo leads cada mes.
- Golden stack probado en ≥5 proyectos; Stack Waiver aplicado ≤1 vez por proyecto.
- Elegir 1 productized service estrella de los 6 y publicar su página con precio.
- **FD Labs:** concluir el scan de repetición; si 3–5 clientes tuvieron el mismo problema → decidir
  el primer producto pequeño (o no). Si no, no construir.
- **Gates para 6–12:** close rate ≥25% en propuestas · churn de retainer ≤12% anualizado ·
  1 sola vertical con ≥3 clientes repetidos · 0 proyectos con scope sin SOW.

### Fase 6–12 meses: "Escalar lo que funciona"
- Mezcla objetivo: **recurrencia ≥40% del ingreso** (mantenimientos + setups con retainer).
- Segunda vertical en test o profundizar la primera (elegir por datos de la fuente #8 del CRM).
- **FD Labs:** máximo 1 producto pequeño en mercado con **pago real desde el día 1** (o 0 si no
  hay señal — archivar es una victoria). PULSAR sigue siendo flagship.
- Evaluar **contrato 1** (account ops / delivery, no junior) si ambos socios >60h/semana con
  flujo estable y pipeline ≥3 meses.
- Revisar tarifarios (6 meses o TC >15%); recategorización monotributo prevista.
- **Gates para el año 2:** reserva de caja 3–6 meses · 1 alianza = ≥15% del pipeline · utilización
  <85% de utilización media (si no, no escalar más proyectos — subcontratar o subir precio).

---

## PARTE 27 — FAILURE MODES (pre-mortem: "fracasó dentro de 18 meses")

Base: CB Insights 2026 (431 empresas VC cerradas desde 2023 — 70% "sin capital" como causa final,
43% PMF, 19% unit economics, casi un tercio murió con ≤10 empleados), Ignition 2025 (97% persigue
cobros tardíos, 63% cashflow impredecible, 57% pierde USD 1–5k/mes en scope creep), post-mortems de
Reddit (r/agency 1ium035, r/Entrepreneur 1974vor, r/DigitalMarketing 1ivtdc2) y HN.

| # | Causa | Early warning | Prevention | Response |
|---|---|---|---|---|
| 1 | Demasiados proyectos | >2 con el mismo par de manos; entregas atrasadas; ≥30% horas sin facturar | WIP 2–3; fecha+precio cerrados; 20% capacidad libre | Cerrar captación 30 días; terminar/rescindir el peor; renegociar fechas por escrito |
| 2 | Pricing bajo | Margen bruto <40%; aceptar precio al primer "sí" | Suelo de precio escrito; paquetes cerrados; +20% cada 10 propuestas | Renegociar los 2–3 peores; rechazar bajo el suelo; eliminar el cliente más barato |
| 3 | Scope creep | "Favors" semanales sin change order; horas >110% del estimado | Alcance por entregable + proceso de cambio + canal único | Facturar exceso; re-baseline o change order por escrito |
| 4 | Dependencia de cold outreach | 1 canal = >60% leads; reply <2% dos meses | 3+ canales; SPF/DKIM/DMARC; campañas <50 | Pausar volumen, re-verificar listas, activar base y pedir referrals |
| 5 | Falta de ventas | Pipeline <3 meses; 0 propuestas en 10 días; founders >80% en delivery | Meta semanal intocable de prospección; CRM; founder-owner de ventas | Bloquear mañanas de ventas; 10 discovery a la base; replantear oferta si 60 días sin pipeline |
| 6 | Sobre-arquitectura | Semanas sin demo; refactor "antes del MVP"; infra con 0 usuarios | Stack aburrido; nada se construye sin pago o 10 entrevistas; demo cada 2 semanas | Congelar ingeniería sin cliente; exigir ≥50% de semanas con algo visible |
| 7 | Demasiados productos | >1 con commits en 2 semanas; ninguno con 100 usuarios o 1 pago | Regla del uno; gate 0–12 meses | Elegir 1 por criterio escrito; archivar el resto con fecha |
| 8 | Soporte devorando | ≥10% horas en tickets sin SLA; >5 interrupciones/día | Fee mensual + SLA + runbook antes de escalar | Cobrar soporte como line item; externalizar L1; migrar o dar de baja lo no pagado |
| 9 | Falta de caja | Runway <3 meses; DSO >45 días; >25% facturas vencidas | Reserva 3–6 meses; 50% anticipo; facturar a entrega; previsión 13 semanas | Cobrar cartera hoy; recortar gastos fijos antes que capacidad de venta |
| 10 | Burnout de founders | Sin día libre 3+ semanas; >50h sostenidas; irritabilidad | Tarifa que paga vacaciones; no-negociables semanales; turnos alternos | Declinar no crítico 2 semanas; externalizar táctico; renegociar reparto |
| 11 | Clientes malos | 1 cliente >40% ingresos; pagos >30 días repetidos; 2+ quejas internas | Criterio ICP + contrato + anticipo; concentración máx. 30% | Salida planificada de los 2 peores; congelar expansiones |
| 12 | Software sin mercado | 0 pagos tras 3 meses; "está bien" en entrevistas; <20% activación | Vender/anticipar antes de construir; 20 entrevistas; kill criteria numéricos | Congelar features; vender la versión tosca a 5 prospectos; 0 compras en 60 días → pivotar o archivar (no refactorizar) |

**3 lecciones cruzadas:** (1) **vender antes de construir** — la capacidad de delivery nunca es la
restricción, la adquisición sí; (2) **un solo canal = riesgo existencial**; (3) **sin reserva de
caja, todo lo demás es irrelevante** — la caja es el techo de tiempo para corregir las otras 11.

---

## PARTE 28 — DECISION FRAMEWORKS

Criterios altos, sin scores numéricos arbitrarios. Detalle ampliado en
`docs/business/project-acceptance-framework.md`.

### 1. ¿Aceptamos este cliente? — GO / INVESTIGATE / NO-GO
- **GO:** encaja en ICP nominado · hay decisor accesible · habla de presupuesto en la primera
  reunión · acepta anticipo y SOW sin pelear · su problema está en nuestra oferta ATTACK ·
  canal de comunicación razonable (email/ticket, no WhatsApp a las 23h).
- **INVESTIGATE:** ICP dudoso · multi-decisor · pide referencia de trabajo similar que no tenemos ·
  pago sin anticipo.
- **NO-GO:** está en la lista de PARTE 3 · no acepta SOW ni anticipo · exige exclusividad o SLA
  24/7 · ya cambió de proveedor 2 veces en 2 años (patrón) · un socio no quiere (veto).

### 2. ¿Aceptamos este proyecto? — GO / INVESTIGATE / NO-GO
- **GO:** ≤1 mes de los 2 fundadores (y capacity libre ≥10h/semana) · alcance escribible en 1
  página · entra en el golden stack (o hay Stack Waiver pagado) · precio cerrado con exclusiones ·
  hito de pago definido · fecha comprometible con el WIP actual.
- **INVESTIGATE:** >1 mes · integración con API de terceros (¿oficial?) · requiere tech fuera
  del stack · cliente pide precio fijo con alcance vago · depende de fecha externa (feria, temporada).
- **NO-GO:** >2 meses de los 2 fundadores · fixed-price sobre alcance desconocido · facturación/
  componente fiscal regulado sin profesional firmante · scope que solo crece ("agreguemos algo cada
  semana") · no hay anticipo.

### 3. ¿Construimos esta feature custom? — GO / NO-GO
- **GO si:** está dentro del SOW (se hace, planilla de horas) **o** es un change order pagado **o**
  es 100% del "thin layer" del cliente y no toca el core reutilizable · ≤2h absorbible documentado.
- **NO-GO si:** toca el core genérico sin compensación (subir al contrato de todos) · es "porque lo
  vi en otro sistema" sin proceso del cliente · es la 3ª vez que la piden distintos → pasa a PARTE 13
  (candidata a módulo).

### 4. ¿Convertimos esto en producto? — GO / INVESTIGATE / NO-GO
- **GO:** los 6 criterios de PARTE 13 (≥3–5 clientes · >40% repetido · canal nombrable · uso
  recurrente · ≥2 pagos · la 4ª repetición no aprende) **y** hay horas reservadas (parómetro de
  PARTE 14) **y** el contrato con los clientes no impide la IP.
- **INVESTIGATE:** 2–3 clientes o falta el canal → validar con 10 entrevistas y 1 pre-venta.
- **NO-GO:** 1 solo cliente pidió · solo sirve con integración profunda · el core depende de datos
  o reglas de negocio privativos del cliente.

### 5. ¿Creamos este SaaS? — GO / NO-GO
- **GO:** los 7 checks de PARTE 16 (dolor recurrente, uso recurrente, **pago antes de codificar
  producción**, mercado accesible, distribución propia, soporte ≤2h/semana, umbral honesto) **y**
  ninguna API externa en el path crítico sin fallback **y** CAC proyectado ≠ paid ads.
- **NO-GO:** hay que explicar por qué es "la nueva cosa X" · el soporte previsible >2h/semana · un
  tercero controla el precio/la existencia del insumo crítico · no sabés nombrar el canal.

### 6. ¿Archivamos un proyecto/producto? — GO / INVESTIGATE
- **GO (archivar):** producto sin pagos ni uso tras 90 días (2 de 3 condiciones de PARTE 14) ·
  proyecto donde el cliente incumple 2 veces el proceso (pago, canal, rondas) · mantenimiento que
  consume >20% de la capacidad sin margen · vertical que tras 6 meses no dio ni 1 cierre.
- **INVESTIGATE:** hay temporaridad (esperar la temporada) · falta 1 dato (ej. precio) para
  decidir → test de 30 días con kill criteria.
- **Cómo archivar:** visiblemente y con fecha de reactivación (no "pausar" eterno); comunicado al
  cliente con plan de salida de datos y accesos (nunca retener).

---

## TOP 10 ACTIONS

1. Definir los 3 paquetes cerrados con precio publicado y exclusiones (usa `service-catalog-2026.md`).
2. Escribir y firmar internamente: SOW, DoD de 12 puntos, change log, acta de handoff.
3. Elegir la vertical ICP #1 (alojamientos) y armar el pitch de una línea + 10 diagnósticos por WhatsApp.
4. Armar la lista de 50 contactos de red + pedir referral nominal a cada cierre.
5. Cerrar 1 alianza con estudio contable/consultor (1 reunión, propuesta concreta de leads).
6. Publicar diagnóstico gratuito/pagado de 30 min como puerta de entrada en web + WhatsApp.
7. Configurar el CRM: 10 etapas, 4 campos obligatorios, Closed Won = anticipo, panel de 8 métricas.
8. Construir el starter repo (golden stack: Postgres + 1 framework + 1 hosting + CI + DoD checklist).
9. Fijar los límites duros: WIP 3, 15–20 mantenimientos, 6h/semana de producto, regla del 70%.
10. Correr el pre-mortem (PARTE 27) con los dos socios y revisar early warnings cada mes.

## THINGS WE SHOULD NOT DO

- No abrir 10 productos: regla del uno, 4 initiatives máximo, 1 grande (PULSAR).
- No aceptar ERP/contabilidad/fintech/médico/enterprise/crítico/24/7/mega-fixed (PARTE 3).
- No vender horas ni presupuestos abiertos; no arrancar sin anticipo; no "revisiones ilimitadas".
- No competir contra $0 (agenda, cotizaciones, inventarios, gestores genéricos).
- No depender de un solo canal de adquisición ni solo de cold outreach.
- No stacks distintos por cliente; no tech exótica; no CMS a medida; no microservicios/K8s "por si
  acaso".
- No retener dominios, cuentas ni credenciales de clientes.
- No cobrar retainer sin entregable continuo definido; no vender "24/7" si no hay guardia.
- No automatizar lo que ocurre 1 vez al mes; no construir un ERP interno ni un CRM paralelo.
- No construir SaaS por pedido de 1 cliente ni sin el pago antes de la primera línea de producción.
- No contratar vendedor/junior antes de tener posicionamiento, proceso y flujo estable.
- No congelar PULSAR para "dar aire" a los proyectos de la agencia.

## UNKNOWNS REQUIRING REAL CLIENT INTERVIEWS

(Validar con 10–20 entrevistas reales antes de decidir precios finales y vertical.)

1. **Vertical final:** ¿alojamientos, inmobiliarias u otra? ¿Cuál duele más *acá* (la evidencia es
   LatAm/AR general; falta la muestra local)?
2. **Willingness to pay local:** ¿el ticket de USD 1.500–3.500 cierra o el techo real es menor?
   (Los anchors son blogs de agencias AR — `WEAK` por ser oferta publicada, no transacción.)
3. **Diagnóstico:** ¿el mercado AR paga un Operations Map de USD 300–450 o solo lo acepta gratis?
4. **Mantenimiento attach rate:** ¿qué % de los clientes nuevos aceptan un plan Care desde el día 1?
   (Ninguna fuente cuantifica el attach en AR.)
5. **Precio del contador/gestoría:** falta tarifa verificada AR 2026.
6. **Alianzas:** ¿los contadores de la zona aceptan un modelo de referidos con incentivo?
7. **Soporte:** ¿qué espera real tiene el cliente PyME AR en horario y canal? (¿48h o "ahora mismo"?)
8. **Inscripción AAIP del CRM propio:** criterio de "destinado a dar informes" → **requiere abogado**.
9. **Estacionalidad:** ¿la temporada de la vertical elegida cuándo exactamente genera los picos de
   disparador de compra?
10. **Producto FD Labs:** los 5 patrones repetidos de PARTE 5 son de foros, no de *nuestros*
    clientes — el scan del primer trimestre es la evidencia que falta.

---

## METHOD NOTES & CAVEATS

- 11 tracks de investigación en paralelo (marca, demanda, no-go, ICP, patrones/productized,
  pricing/ventas/CRM, delivery/stack/retainers, factory/portfolio/OSS/SaaS, distribución/economía,
  legal/automatización, pre-mortem). Evidencia preferente 2025–2026.
- `websearch` falló con HTTP 401 en varias corridas → fallback: Algolia HN API, GitHub API, pullpush
  (Reddit), fetch directo, DuckDuckGo/Brave HTML. Reddit bloqueó fetch directo (403/429): mucho
  material Reddit es **snippet** (marcado `WEAK` donde afecta).
- Blogs de vendors (precios de agencias, benchmarks de agencias) son `HECHO` de *precio publicado*
  pero `WEAK` como muestra; los números de CAC/close/utilización de agregadores se usan como orden
  de magnitud.
- Product Hunt/listicles no se usaron como evidencia de dolor; los pricing pages solo en COMPETITION.
- Búsquedas negativas declaradas: no se hallaron post-mortems 2025–26 de agencias quebradas por
  fixed-price ni threads de "mantengo 40 proyectos" con esa frase literal — esas premisas quedan
  marcadas `INFERENCIA`.

## FILES CREATED

- `docs/business/solutions-tech-fd-labs-masterplan-2026.md` (este documento)
- `docs/business/service-catalog-2026.md`
- `docs/business/project-acceptance-framework.md`
