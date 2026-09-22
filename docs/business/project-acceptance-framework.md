# Solutions Tech — Framework de aceptación de clientes y proyectos

> Compañero de `docs/business/solutions-tech-fd-labs-masterplan-2026.md` (PARTE 3 y PARTE 28).
> Criterios duros, no intuición: matrices GO / INVESTIGATE / NO-GO y la no-go list con el riesgo
> específico de cada categoría. `RECOMENDACIÓN` todo el documento salvo indicación; lo marcado
> `HECHO` tiene fuente recuperada (leyes, ToS, noticias, threads).
>
> **Uso:** una página, dos personas, decisiones en 10 minutos antes de cualquier propuesta.
> Este documento **no es asesoría jurídica** — lo marcado [ABOGADO]/[CONTADOR] requiere
> profesional local (ver masterplan PARTE 22).

---

## 1. NO-GO LIST — 14 trabajos a evitar (12 meses) con el riesgo real

| # | Trabajo | Riesgo principal | Riesgo secundario | Veredicto 12m | Condición para GO a futuro |
|---|---|---|---|---|---|
| 1 | **ERP completo propio** | Técno/soporte: scope infinito + 55–75% de implementaciones ERP fallan por presupuesto/plazo (Gartner vía HN 33864756, `WEAK` por fecha) | Un solo "dueño del dominio contable" = bus factor 1 | **NO-GO** | Implementar/parametrizar ERP existente (Odoo, ERPnube), por horas, con estudio contable respaldando los criterios impositivos |
| 2 | **Sistemas contables propios** | Fiscal: error de cálculo (IVA, retenciones, IIBB) → autofactura incorrecta y sanción al cliente que te reclama | El contador firma el balance; vos firmaste el código | **NO-GO** (módulo propio) / **INVESTIGATE** (integración) | Contador colegiado firma la spec fiscal; vos solo ejecutás; DPA de datos (Ley 25.326 art. 25) |
| 3 | **Fintech** | Legal: régimen BCRA (Registro de proveedores de servicios de pago — `HECHO`, bcra.gob.ar) | Si tu sistema procesa mal un pago, la pérdida la busca alguien | **NO-GO** | Vendor de un PSP ya inscripto, con exclusión de responsabilidad operativa escrita |
| 4 | **Software médico regulado** | Legal/RC: ANMAT (GHEMA/HELENA/THEMIS, Disp. 8799/2025) + ensayos IEC + vigilancia post-venta | Un error clínico por software es titular y causa civil | **NO-GO** | Solo software no clínico (agendas, facturación de consultorios), o bajo responsable técnico registrado; equipo ≥6 |
| 5 | **POS fiscal (CAI/ARCA)** | Fiscal: numeración, CAI y controladores homologados — el POS es crítico en el mostrador (si no factura, no vende) | Soporte regulatorio continuo | **INVESTIGATE** | Desarrollar solo contra e-facturación existente; alta de puntos de venta y CAI = cliente (guías ARCA `HECHO`); hardware por integrador homologado (RG 3.561, `WEAK`) |
| 6 | **Marketplaces** | Negocio: cold start de dos lados ("cold starts are hard" — HN 49593890) | Custodia de pagos, chargebacks, disputas de usuarios | **INVESTIGATE** | Un lado ya líquido, sin custodia de fondos (pasar por PSP), por fases con roadmap de liquidez firmado |
| 7 | **Redes sociales / clones** | Reputacional: moderación 24/7, abuso, estafas, niños | Ley 25.326 a escala + dependencia de tu única plataforma | **NO-GO** | Comunidades privadas/grupos con moderación acotada y política de contenido a cargo del cliente |
| 8 | **Proyectos enterprise** | Cobro: procurement, net-90, auditorías de proveedor; bus factor 1 en un comité de compras | NDAs, penalidades, seguros, compliance | **NO-GO** | ≥5 personas, capital para 90 días de cobro, seguro RC/ciber, **penalidad topeada = honorarios** |
| 9 | **Infraestructura crítica** | SLA/RC: sin rotación real — "mínimo 3–4 personas para un 24/7 sano" (HN 10892879, equipo real de 6, `WEAK` por fecha) | Acceso total a sistemas ajenos = incidente percibido tuyo | **NO-GO** | ≥4 personas con guardia, monitoreo y runbooks, tope de responsabilidad, exclusión de fallas del proveedor, seguro |
| 10 | **Scraping dudoso** | Legal: demanda cara aunque tengas razón (hiQ v. LinkedIn no es "scraping siempre legal"; saltar login = ilegal; Reddit v. Perplexity 2025–26, X/Meta v. Bright Data) | Te bloquean las fuentes a mitad de proyecto | **NO-GO** | Solo APIs públicas, datos con licenciados o datos del propio cliente + cláusula de garantía de derechos del cliente |
| 11 | **Automatizaciones con APIs no oficiales** | Técnico: rompen sin changelog ni aviso ("So an app that uses an unofficial API broke when that API changed? Not news" — HN 49524568) | Violación de ToS; mantenimiento perpetuo no facturado | **NO-GO** | Solo APIs oficiales con SLA publicado y presupuesto de mantenimiento explícito; si no hay API oficial → decir no y ofrecer export/manual |
| 12 | **WhatsApp/Instagram no autorizados** | Plataforma: baneo de la cuenta de WhatsApp Business del cliente — y te culpa a vos (política oficial `HECHO`, whatsappbusiness.com/policy: 24 h, plantillas, procedimiento de suspensión) | Costo por conversación de la API oficial no presupuestado; Meta puede restringir cuentas | **NO-GO** | Solo Cloud API oficial / Instagram Graph API con Business account, plantillas aprobadas, escalamiento humano, presupuesto de conversaciones |
| 13 | **Soporte 24/7** | Personas: con 2 personas no hay guardia → uno de los dos es la guardia siempre | SLA con penalidad = quiebra de contrato; sin seguro ni redundancia | **NO-GO** | Soporte con horario + SLA en días hábiles; 24/7 real solo vía NOC subcontratado back-to-back con ≥4 en planta |
| 14 | **Proyecto enorme a precio fijo** | Cashflow/scope: "Fixed price projects are like handling dynamite" (HN 46500935) — el cliente sofisticado extrae mucho trabajo del consultor que no sabe poner límites | Asimetría legal: un cliente con USD 6k pagados te amenaza con demandar por USD 100k (HN 9365020, `WEAK` por fecha) | **NO-GO** si >2 meses de los 2 fundadores o >50% de la capacidad trimestral | Time & materials o hitos con adelanto; fixed solo con descubrimiento pagado, alcance cerrado, change orders, retención final, IP liberada solo con pago total, penalidad topeada |

**Ningún NO-GO es permanente.** Todos pasan a GO con una de: equipo ≥4–6, seguro, profesional
firmante (contador/RT), subcontratación de la parte regulada o cambio de modelo de cobro.

**Las 3 reglas duras que resuelven la mayoría:**
1. *Si el cliente queda en infracción ante ARCA/BCRA/ANMAT por algo que vos construiste, la
   respuesta contractual tiene que estar escrita antes de firmar.*
2. *Si el mantenimiento no está presupuestado explícitamente, no es un proyecto: es una
   suscripción gratis.*
3. *Si la entrega depende de que una sola de las dos personas esté disponible a las 3 a.m., el
   SLA es mentira.*

---

## 2. ¿ACEPTAMOS ESTE CLIENTE? — GO / INVESTIGATE / NO-GO

**GO si cumple todo:**
- Encaja en un ICP **nominado** (no "PyMEs" — ver masterplan PARTE 4).
- Hay **decisor accesible** en la primera reunión.
- Habla de **presupuesto** en la primera reunión (rango, no "lo que salga").
- Acepta **anticipo y SOW** sin pelear.
- Su problema está en la oferta **ATTACK** (o en un TEST ya probado ≥2 veces).
- Canal de comunicación razonable: email/ticket, no WhatsApp a las 23h.

**INVESTIGATE si:**
- ICP dudoso o mezcla de rubros sin proceso común.
- Multi-decisor sin definir quién firma.
- Pide referencia de trabajo similar que no tenemos (→ ofrecer Operations Map).
- Propone pago sin anticipo ("facturamos a 60 días").

**NO-GO si:**
- Está en la NO-GO list de la sección 1 (cliente o tipo de trabajo).
- No acepta SOW ni anticipo.
- Exige exclusividad, soporte 24/7 o SLA con penalidad.
- Ya cambió de proveedor 2 veces en 2 años (patrón de "cliente malo" — post-mortems).
- Cualquiera de los dos socios dice no (**veto por dominio**: Jaz veto comercial, Fran veto técnico).

---

## 3. ¿ACEPTAMOS ESTE PROYECTO? — GO / INVESTIGATE / NO-GO

**GO si:**
- ≤1 mes de trabajo de los 2 fundadores **y** capacity libre ≥10h/semana en el tablero.
- Alcance escribible en 1 página (entregables numerados + exclusiones).
- Entra en el **golden stack** (o hay Stack Waiver pagado aprobado por Fran).
- Precio cerrado con lista OUT explícita + change order.
- Hitos de pago definidos (50/25/25) y fecha comprometible con el WIP actual.

**INVESTIGATE si:**
- Dura >1 mes (repartir en fases con anticipo por fase).
- Requiere integración con API de tercera parte → **¿es oficial? ¿tiene SLA? ¿qué pasa si cae?**
- Requiere tecnología fuera del stack → ¿Stack Waiver + costo extra?
- Cliente pide precio fijo con alcance vago → convertir a paquete con IN/OUT o rechazar el fixed.
- Depende de una fecha externa (feria, temporada) → margen de contingencia escrito.

**NO-GO si:**
- >2 meses de los 2 fundadores a precio fijo.
- Fixed-price sobre alcance desconocido ("terminarlo y vemos").
- Incluye componente fiscal/regulado sin profesional firmante.
- Scope que solo crece ("agregamos una cosa por semana") → change orders o no.
- No hay anticipo firmado.

---

## 4. ¿CONSTRUIMOS ESTA FEATURE CUSTOM? — GO / NO-GO

**GO si:**
- Está dentro del SOW → se hace, se anota en la planilla de horas (no es "cambio").
- Es un **change order** pagado (presupuesto ≤2 días hábiles, aprobación escrita).
- Es 100% "thin layer" del cliente (campos, flujos, copy) y no toca el core reutilizable.
- ≤2h absorbible → se hace, se documenta por email y se registra en el Change Log.

**NO-GO si:**
- Toca el core genérico sin compensación para todos los demás clientes (pasa a PARTE 13 del
  masterplan: ¿3 clientes con lo mismo? → entonces es módulo, no custom).
- Es "porque lo vi en otro sistema" sin un proceso del cliente que lo justifique.
- Es la 3ª vez que la piden distintos clientes → **GO como módulo**, NO-GO como custom repetido.

*Frase de referencia ante el pedido sin cambio de alcance: "¿Querés que te arme el presupuesto de
eso?" (r/agencynewbies: si cambia el entregable es scope change, aunque tome 20 minutos).*

---

## 5. ¿CONVERTIMOS ESTO EN PRODUCTO? — GO / INVESTIGATE / NO-GO

**GO solo si cumplen LOS 6 (masterplan PARTE 13) y:**
1. **≥3–5 clientes distintos** (no relacionados) con el mismo problema raíz.
2. **>40%** del entregable es contenido repetido.
3. Segmento homogéneo con **canal de distribución nombrable** (si no lo nombrás, no hay canal).
4. El problema **vuelve** (uso recurrente, no proyecto único).
5. **≥2 clientes ya pagaron** una versión semi-repetida.
6. La 4ª repetición **no aporta conocimiento nuevo**.
7. **Y** hay horas reservadas en el parómetro de producto (PARTE 14: 10–14h/semana reales) **y** el
   contrato con los clientes no impide la IP.

**INVESTIGATE si:** solo 2–3 clientes o falta el canal → validar con 10 entrevistas + 1 pre-venta
antes de escribir código.

**NO-GO si:**
- 1 solo cliente pidió el producto (= encargo con factura de SaaS).
- Solo sirve con integración profunda a un cliente.
- El core depende de datos o reglas de negocio privativos del cliente (no se puede separar).

**Cómo hacerlo sin apropiarse de nada:** reescribir (test clean-room), datos del cliente nunca
entran al módulo, cláusula de IP desde hoy (motor genérico vs caso específico), NDA = conocimiento
sí / material no, validar con el cliente si el producto compite en su mercado.

---

## 6. ¿CREAMOS ESTE SAAS? — GO / NO-GO

**GO solo si cumplen LOS 7 (masterplan PARTE 16) y:**
1. **Dolor recurrente** (el problema aparece ≥mensualmente en 3–5 clientes del patrón).
2. **Uso recurrente** real (si no vuelve semanal/mensualmente, no es SaaS).
3. **≥1 pago antes de escribir código de producción** (Freemius 2025: opt-out ~50% de conversión
   vs freemium 3–4%; ~70% de founders piden tarjeta desde el día 1).
4. Mercado accesible: podés nombrar el rubro y el canal.
5. Distribución propia (base de Solutions Tech, comunidad OSS, lista, SEO).
6. Mantenimiento soportable: ≤2h/semana en estado estacionario.
7. Umbral honesto: mediana de micro-SaaS rentable ≈ USD 4,2k MRR; ~70% queda bajo USD 1k →
   medir también valor estratégico (datos, referencia, canal).
8. **Ninguna API externa en el path crítico sin fallback** (Bing agosto-2025, DeepSeek 2026, X Vid
   feb-2025: APIs mueren) · **CAC proyectado ≠ paid ads** (benchmarks: CAC mediano USD 702,
   payback 17,3 meses).

**NO-GO si:**
- Hay que explicar por qué es "la nueva cosa X".
- El soporte previsible >2h/semana o requiere tiers de soporte desde el día 1.
- Un tercero controla el precio o la existencia del insumo crítico y no hay plan de migración.
- No sabés nombrar el canal de adquisición.
- El dolor no vuelve todos los meses (entonces es proyecto, no SaaS).

---

## 7. ¿ARCHIVAMOS UN PROYECTO / PRODUCTO? — GO / INVESTIGATE

**GO (archivar) si:**
- **Producto:** sin pagos ni uso creciente tras **90 días** (cumple 2 de 3: 0 clientes pagos · sin
  uso semanal creciente · consume >50% del presupuesto de producto sin señal).
- **Cliente:** incumple 2 veces el proceso (pago, canal, rondas de revisión).
- **Mantenimiento:** consume >20% de la capacidad sin margen.
- **Vertical:** tras 6 meses de test, 0 cierres (ej: gastronomía, comercio minorista).

**INVESTIGATE si:**
- Hay estacionalidad (esperar 1 temporada con fecha de revisión).
- Falta 1 dato para decidir (p. ej. precio) → test de 30 días con kill criteria numéricos escritos
  ("si en 30 días no pasa X, se archiva").

**Cómo archivar (siempre):** visiblemente y con fecha de reactivación (no "pausar" eterno);
comunicado al cliente con plan de salida de datos y accesos; **nunca retener credenciales ni
dominios**; backup final entregado.

---

## 8. Checklist previo a firmar (10 preguntas, 10 minutos)

1. ¿El cliente está en un ICP nominado? □
2. ¿Habla de presupuesto en la primera reunión? □
3. ¿Hay decisor en la call y firma él? □
4. ¿El tipo de trabajo está en la NO-GO list? (si sí → no pasa de aquí) □
5. ¿≤1 mes de los 2 fundadores y capacity libre ≥10h/semana? □
6. ¿Alcance escribible en 1 página con lista OUT? □
7. ¿Precio cerrado + anticipo 50% + validez 15 días? □
8. ¿Entra en el golden stack (o Stack Waiver pagado)? □
9. ¿Anticipo y SOW aceptados? (Closed Won = anticipo acreditado) □
10. ¿Oferta de mantenimiento adjunta al cierre? □

Si 1–5 fallan → no presentar propuesta. Si 6–10 fallan → corregir propuesta antes de enviar.

---

## Método y caveats

- Evidencia preferente 2025–2026: texto oficial (ARCA, BCRA, ANMAT, Ley 25.326, ToS de Meta),
  threads de HN vía Algolia API, post-mortems de Reddit (pullpush — el fetch directo de Reddit
  devolvió 403), news (`WEAK` cuando el fetch fue 401/403 y solo se tuvo el título).
- Los veredictos a 12 meses son `RECOMENDACIÓN` basada en esos riesgos; la reversibilidad de cada
  NO-GO es el punto: nada aquí es para siempre, todo tiene su condición de GO.
