#!/usr/bin/env bash
# EscapeTheUAB — Issues de presentación (3 equipos) y multiplayer LAN.
# No modifica archivos del repositorio, ramas, commits ni pull requests.
set -euo pipefail

REPO='Francois0405/EscapeTheUAB'
MODE="${1:-create}"

if [[ "$MODE" != "create" && "$MODE" != "--dry-run" ]]; then
  printf 'Uso: bash %s [--dry-run]\n' "$0" >&2
  exit 2
fi

if [[ "$MODE" == "create" ]]; then
  if ! command -v gh >/dev/null 2>&1; then
    echo 'No se encuentra GitHub CLI (gh). En macOS: brew install gh; después: gh auth login' >&2
    exit 1
  fi
  if ! gh auth status >/dev/null 2>&1; then
    echo 'GitHub CLI no tiene sesión válida. Ejecuta: gh auth login' >&2
    exit 1
  fi
  echo "Repositorio de destino: $REPO"
  gh repo view "$REPO" --json nameWithOwner --jq '.nameWithOwner' >/dev/null
fi

create_issue() {
  local title="$1"
  if [[ "$MODE" == "--dry-run" ]]; then
    cat >/dev/null
    printf 'PREVISUALIZACIÓN: %s\n' "$title"
    return 0
  fi

  local existing
  existing="$(gh issue list --repo "$REPO" --state all --limit 300 --json title,url --jq ".[] | select(.title == \"$title\") | .url")"
  if [[ -n "$existing" ]]; then
    cat >/dev/null
    printf 'EXISTE: %s\n  %s\n' "$title" "$existing"
    return 0
  fi

  local body_file
  body_file="$(mktemp)"
  cat > "$body_file"
  printf '\nCreando: %s\n' "$title"
  if gh issue create --repo "$REPO" --title "$title" --body-file "$body_file"; then
    rm -f "$body_file"
  else
    rm -f "$body_file"
    echo 'Error creando issue; ninguna rama ni commit ha sido alterado.' >&2
    return 1
  fi
}

create_issue '[P1][Presentación] Guion de la defensa y diapositivas en Canva' <<'ISSUE_1'
<!-- EscapeTheUAB-demo-2026-11-23:guion -->
## Objetivo
Preparar una defensa clara y convincente de EscapeTheUAB en **Canva**, coordinada con la demo técnica. Uno de los **tres ordenadores** se dedica exclusivamente a las diapositivas; los otros dos se utilizan para VR y teclado/ratón.

## Pendiente
- [ ] Confirmar con el profesor/equipo fecha, duración, rúbrica y si se permite utilizar 3 equipos y cambiar fuentes del proyector. No suponer todavía una fecha ni un tiempo de exposición.
- [ ] Acordar roles: portavoz de Canva, operador de VR/Quest 3, jugador PC y persona de apoyo durante cambios de fuente.
- [ ] Diseñar guion con: problema/objetivo del escape room; arquitectura C++/OpenGL/GLSL, GLM, Bullet y OpenXR; creación del escenario, materiales/iluminación, animaciones, interacción, retos técnicos, demo y conclusiones.
- [ ] Diferenciar funciones **demostradas con hardware** de funciones implementadas pero aún sin prueba real. No presentar multiplayer ni OpenXR como validados hasta que existan resultados.
- [ ] Crear diapositivas legibles desde el fondo del aula, con capturas del juego reales y referencias a contribuciones individuales.
- [ ] Redactar frases de transición entre Canva, espejo VR y jugador PC; preparar recorrido jugable acotado.
- [ ] Exportar una copia PDF de Canva para uso sin Internet; conservar una copia local del guion y un vídeo breve de la demo como respaldo.
- [ ] Ensayar exposición y preguntas con un cronómetro, dentro del tiempo asignado.

## Criterios de aceptación
Canva terminada y revisada por el equipo; guion con responsables y tiempos; exportación offline verificada; las afirmaciones técnicas coinciden con lo probado y el público entiende qué está viendo en VR y en escritorio.

**Dependencias:** #14 (prueba real VR), #15 (QA). **Relacionada con:** issue de montaje de 3 equipos y viabilidad LAN. Canva no forma parte de la conexión multijugador.
ISSUE_1

create_issue '[P1][Demo] Montaje y ensayo de 3 ordenadores: Canva, Quest 3 y teclado/ratón' <<'ISSUE_2'
<!-- EscapeTheUAB-demo-2026-11-23:hardware -->
## Objetivo
Conseguir una presentación reproducible con **3 ordenadores físicos**:
1. **Canva:** diapositivas y control de la exposición; no ejecuta el juego ni participa en red LAN del juego.
2. **VR:** PC Windows con EscapeTheUAB, Meta Quest 3 y runtime OpenXR/Quest Link, con espejo de la imagen del visor si lo permite la configuración.
3. **FPS escritorio:** otro ordenador con EscapeTheUAB, teclado y ratón. En la demostración base puede ser una partida independiente; si se aprueba el MVP LAN, se conecta como segundo jugador.

## Checklist de preparación
- [ ] Identificar qué ordenador hará cada función y quién lo controla; comprobar requisitos de hardware, SO, drivers, alimentación y compatibilidad de la sala.
- [ ] Verificar enlace a proyector/pantalla, adaptadores HDMI/USB-C, conmutación entre fuentes (o capturadora/mezclador si se dispone), audio, cableado, alargadores y posición del público. No asumir que el proyector acepta tres entradas simultáneas.
- [ ] Configurar resolución, escalado y modo espejo de Quest para que el público vea una imagen útil, sin exponer herramientas de desarrollo ni ventanas inesperadas.
- [ ] Ejecutar prueba REAL con Quest 3 de #14: imagen binocular, seguimiento, controles, recenter, seguridad del espacio físico y estabilidad. F2 estereoscópico en monitor no equivale a VR.
- [ ] Verificar build Release y recursos en los dos equipos de juego (mismo estado de assets y commit o builds compatibles); ejecutar pruebas funcionales de #15.
- [ ] Medir rendimiento y tirones de FPS/VR por separado. Mantener la fluidez/comfort VR como requisito de demo.
- [ ] Si existe MVP multiplayer aprobado, conectar ambos equipos de juego mediante LAN local fiable (Ethernet preferente) y ensayar la conexión sin depender del Internet de la universidad. El equipo Canva permanece independiente.
- [ ] Ensayar cambios de presentación Canva → visor VR (espejo) → PC teclado/ratón → cierre, sin cables atravesando zona de movimiento del visor.
- [ ] Disponer de plan B: demo solo teclado/ratón, modo VR en solitario o vídeo grabado si fallan casco, proyector o LAN.
- [ ] Congelar una versión probada y realizar al menos un ensayo completo en la configuración más parecida posible al aula.

## Criterios de aceptación
Los 3 equipos pueden funcionar simultáneamente de forma estable, los cambios de fuente y audio están documentados, la ruta de demostración es reproducible y existe un plan B ensayado que **no depende del multijugador**.

**Dependencias:** #14 (VR hardware), #15 (QA). **Opcional:** MVP multiplayer LAN. **Responsables:** equipo, pendientes de repartir.
ISSUE_2

create_issue '[P1][Networking] Spike GO/NO-GO para multijugador LAN PC + VR' <<'ISSUE_3'
<!-- EscapeTheUAB-demo-2026-11-23:network-spike -->
## Objetivo
Determinar con evidencia si es realista enseñar **dos jugadores compartiendo escena**, uno con teclado/ratón en un ordenador y otro con Meta Quest 3 conectado a un segundo PC Windows, durante la presentación.

**Alcance: investigación y prueba vertical mínima. No implementar todavía todo el cooperativo.** La tercera máquina solo reproduce Canva y no es cliente del juego.

## Diagnóstico y diseño
- [ ] Auditar HEAD actual y confirmar módulos de OpenXR, entrada FPS/VR, Bullet, puertas GLB, IDs de entidades, ciclo de actualización y estados. Distinguir código compilado de función validada en visor.
- [ ] Completar antes la prueba real de Quest 3 de #14. Si VR falla, se puede estudiar primero LAN escritorio-escritorio, pero no declarar viable la presentación PC+VR.
- [ ] Evaluar ENet (UDP fiable/no fiable) frente a alternativas, con soporte Windows, integración CMake/vcpkg y licencia; seleccionar la opción más simple.
- [ ] Diseñar arquitectura **host autoritativo + cliente** para dos jugadores LAN. Fijar autoridad de puertas, objetos y puzles, IDs, versionado de mensajes, validación y tolerancia a desconexión.
- [ ] Separar estado de simulación del renderizado: cada equipo dibuja la escena localmente. No transmitir vídeo por la red del juego ni sincronizar cada vértice.
- [ ] Probar descubrimiento/conexión por IP local, handshake/versiones, intercambio de posición/orientación y un evento de puerta, inicialmente en dos escritorios si es necesario.
- [ ] Estimar latencia, pérdida/orden de paquetes, cargas de CPU y riesgo de bloqueo del render. Establecer frecuencias adecuadas para poses (p. ej. 20–30 Hz iniciales) y eventos fiables.
- [ ] Comprobar que se puede desactivar completamente el módulo LAN y seguir en modo un jugador.
- [ ] Registrar limitaciones: Quest Link, compatibilidad OpenGL, setup del aula, reintento y desincronización de Bullet.

## Puerta de decisión GO/NO-GO
**GO al MVP** solo si hay: prueba VR real satisfactoria en #14, dos equipos que se conectan en LAN de forma repetible, intercambio de estado sin bloqueos, puerta sincronizada en prototipo y plan B monojugador.
**NO-GO / aplazar** si falla cualquiera de los requisitos críticos o amenaza estabilidad/fecha de entrega. No ampliar alcance sin medirlo.

## Entregables / aceptación
Documento corto de arquitectura + informe de pruebas, hardware y commit exacto; decisión GO/NO-GO explícita y esfuerzo pendiente. **No abrir PR a master de un prototipo no validado.**

**Dependencias:** #14. **Relacionadas:** #10 puzles, #12 victoria/reinicio, #15 QA, issue de demo 3 equipos.
ISSUE_3

create_issue '[P2][Multijugador] MVP LAN cooperativo 2 jugadores escritorio + Quest VR (condicionado a GO)' <<'ISSUE_4'
<!-- EscapeTheUAB-demo-2026-11-23:multiplayer-mvp -->
## Objetivo
Si el spike de networking concluye **GO**, entregar una demo cooperativa mínima con **2 instancias del juego** en LAN: una en PC con teclado/ratón y otra en PC con Quest 3 conectadas por OpenXR. El tercer ordenador de Canva es independiente.

**Funcionalidad opcional para presentación: nunca bloquear la entrega individual.**

## MVP estricto
- [ ] Crear host/cliente LAN con conexión/reconexión y diagnóstico básico; no añadir servidores públicos, matchmaking ni chat de voz.
- [ ] Cargar el mismo nivel y representar dos jugadores distintos con identificadores persistentes durante la sesión. El jugador remoto debe verse con un avatar/cápsula sencilla, no con la cámara duplicada.
- [ ] Enviar posiciones y orientaciones de jugador por mensajes no fiables secuenciados; interpolar visualmente el avatar remoto. En VR, agregar cabeza/mandos solo si la sesión y tracking real están comprobados.
- [ ] Replicar de forma **autoritaria** al menos un evento de puerta: abrir/cerrar desde uno y observar exactamente el mismo estado/animación en el otro.
- [ ] Implementar una sola interacción cooperativa adicional **solo si sobra margen**: objeto compartido o evento de puzle discreto, sin sincronizar toda la física Bullet de inmediato.
- [ ] Resolver estados de entrar/salir, reset, pérdida de conexión y stop del host sin corromper escena, objetos, puertas ni modo individual.
- [ ] Conservar física, colisiones e interacción local de ambos jugadores. Evitar esperar paquetes en el hilo de render y limitar efectos secundarios de eventos duplicados.
- [ ] Añadir un interruptor de modo LAN y una ruta de rollback instantánea al modo monojugador.

## Pruebas / criterios de aceptación
- [ ] Dos equipos físicos se conectan a la misma partida durante un ensayo repetido.
- [ ] Ambos ven al otro desplazarse y girar sin saltos graves; en VR, orientación y escala correctas.
- [ ] Abrir/cerrar una puerta se refleja en ambos clientes y sigue funcionando tras una reconexión controlada.
- [ ] El modo monojugador permanece funcional; build Release y CTest pasan, con resultados documentados.
- [ ] Ensayo con el tercer equipo mostrando Canva sin afectar la red; demostración y plan B completados.

**Dependencias obligatorias:** spike de viabilidad LAN con GO + #14 VR hardware. **Relacionadas:** #3 interacciones, #10 puzles, #12 reset/victoria, #15 QA y issue de montaje 3 equipos.

**Fuera de alcance inicial:** multiplayer por Internet, inventario complejo, sincronización determinista total de Bullet, múltiples visores, IA distribuida y modo competitivo. No mezclar con la rama de optimización OpenGL.
ISSUE_4

printf '\nFinalizado. No se ha ejecutado git commit, push, merge ni cambiado ninguna rama.\n'
