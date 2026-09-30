![Bandolibre keyboard](documentation/images/3dmodel_keyboard_tilt.webp)

🇬🇧 [English](README.md) | 🇫🇷 [Français](README.fr.md) | 🇪🇸 **Español**

# Bandolibre

Un controlador MIDI de código abierto que trae el bandoneón comunmente utilizado en Argentina a la era digital.

**Bandolibre tiene un único objetivo:** convertir las pulsaciones de teclas y el movimiento del fuelle en señal MIDI — con fidelidad, sin latencia y sin el costo ni el ruido de un instrumento acústico. Conéctalo a cualquier computadora, tableta o teléfono por USB y estará listo para tocar, componer o practicar.

Una iniciativa abierta de [L'Atelier du bandonéon libre](https://github.com/bandolibre)

---

## Escúchalo

[![Demo del Bandolibre con un clarinete virtual](https://img.youtube.com/vi/6s1wlRKlAk4/maxresdefault.jpg)](https://youtu.be/6s1wlRKlAk4)

Mira las demos: [con un clarinete virtual](https://youtu.be/6s1wlRKlAk4) · [con instrumentos virtuales de cuerda](https://youtu.be/nJ0j7DtbYDk).

---

## ¿Para quién es?

- **Estudiantes** que aún no tienen instrumento y quieren comenzar a aprender
- **Músicos** que quieren practicar en silencio — en casa, de viaje, a cualquier hora
- **Compositores** que ingresan partituras nota a nota en un software de notación
- **Músicos de escenario** que controlan sintetizadores y sampleadores en vivo o en estudio

---

## ¿Cómo funciona?

- **Teclado Rheinische Lage de 142 tonos** — ambas manos, digitación exacta del bandoneón tradicional en Argentina
- **Sensores de efecto Hall en cada tecla** — sin contacto mecánico, sin desgaste, 2400 lecturas por segundo
- **Lámina de resorte + sensor** para el fuelle — mide el esfuerzo de empuje y de tracción y emite MIDI CC#11 (Expresión), igual que el instrumento real
- **Dos entradas de pedal de expresión de 6,35 mm** — compatibles con M-Audio EX-P; pedal 1 envía CC#1 (Modulación), pedal 2 envía CC#4 (Controlador de pie)
- **USB-MIDI** de fábrica — conéctalo a cualquier DAW, software de notación o sintetizador; no necesita controladores

---

## Funcionalidades

**Sintonización del teclado** — un botón para ciclar entre las tres disposiciones de teclado: Rheinische Tonlage (bisonórico, 142 tonos), Peguri o Manoury.

**Modo mesa** — un botón activa el modo mesa: las teclas suenan inmediatamente a velocidad fija, sin necesidad de mover el fuelle. Ideal para ingresar una partitura nota a nota sin accionar el fuelle.

**Toca en cualquier lugar** — Bandolibre se alimenta por el bus USB; cualquier teléfono, tableta o laptop con un sintetizador por software se convierte en el motor de sonido. Un pequeño hub USB con salida de auriculares y paso de alimentación te da audio, carga y MIDI desde un solo cable — testeado y de bolsillo.

**Adaptable** — el estándar USB-MIDI es compatible con todo el ecosistema de adaptadores: conecta un adaptador USB Bluetooth MIDI para tocar de forma inalámbrica, o un adaptador USB a DIN-5 para controlar sintetizadores de hardware vintage.

<a href="documentation/user_manual.es.md"><img width="277" alt="Manual de usuario" src="https://img.shields.io/badge/Manual_de_usuario-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1NzYgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTU0Mi4yMiAzMi4wNWMtNTQuOCAzLjExLTE2My43MiAxNC40My0yMzAuOTYgNTUuNTktNC42NCAyLjg0LTcuMjcgNy44OS03LjI3IDEzLjE3djM2My44N2MwIDExLjU1IDEyLjYzIDE4Ljg1IDIzLjI4IDEzLjQ5IDY5LjE4LTM0LjgyIDE2OS4yMy00NC4zMiAyMTguNy00Ni45MiAxNi44OS0uODkgMzAuMDItMTQuNDMgMzAuMDItMzAuNjZWNjIuNzVjLjAxLTE3LjcxLTE1LjM1LTMxLjc0LTMzLjc3LTMwLjd6TTI2NC43MyA4Ny42NEMxOTcuNSA0Ni40OCA4OC41OCAzNS4xNyAzMy43OCAzMi4wNSAxNS4zNiAzMS4wMSAwIDQ1LjA0IDAgNjIuNzVWNDAwLjZjMCAxNi4yNCAxMy4xMyAyOS43OCAzMC4wMiAzMC42NiA0OS40OSAyLjYgMTQ5LjU5IDEyLjExIDIxOC43NyA0Ni45NSAxMC42MiA1LjM1IDIzLjIxLTEuOTQgMjMuMjEtMTMuNDZWMTAwLjYzYzAtNS4yOS0yLjYyLTEwLjE0LTcuMjctMTIuOTl6Ii8+PC9zdmc+"></a>

---

## Cualquiera puede construirlo

Este es un proyecto DIY completamente abierto. Los PCB están diseñados para la fabricación "economic PCBA" JLCPCB de dos capas — accesibles y fáciles de pedir. Las piezas mecánicas son imprimibles en 3D (un FabLab cercano funciona de maravilla). El firmware es de código abierto y se puede flashear con una sonda [ST-LINK](https://www.st.com/en/development-tools/stlink-v3minie.html) estándar y un cable [TC-2070-IDC-050](https://www.tag-connect.com/product/tc2070-idc-050).

Construir un Bandolibre cuesta aproximadamente lo mismo que un buen teclado MIDI o un buen par de auriculares de estudio como el DT-770 Pro.

La lista de materiales, las herramientas, los consumibles y el montaje paso a paso están en las instrucciones de montaje:

<a href="documentation/assembly_instructions.md"><img width="363" alt="Instrucciones de montaje" src="https://img.shields.io/badge/Instrucciones_de_montaje-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1NzYgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTU3MS4zMSAxOTMuOTRsLTIyLjYzLTIyLjYzYy02LjI1LTYuMjUtMTYuMzgtNi4yNS0yMi42MyAwbC0xMS4zMSAxMS4zMS0yOC45LTI4LjljNS42My0yMS4zMS4zNi00NC45LTE2LjM1LTYxLjYxbC00NS4yNS00NS4yNWMtNjIuNDgtNjIuNDgtMTYzLjc5LTYyLjQ4LTIyNi4yOCAwbDkwLjUxIDQ1LjI1djE4Ljc1YzAgMTYuOTcgNi43NCAzMy4yNSAxOC43NSA0NS4yNWw0OS4xNCA0OS4xNGMxNi43MSAxNi43MSA0MC4zIDIxLjk4IDYxLjYxIDE2LjM1bDI4LjkgMjguOS0xMS4zMSAxMS4zMWMtNi4yNSA2LjI1LTYuMjUgMTYuMzggMCAyMi42M2wyMi42MyAyMi42M2M2LjI1IDYuMjUgMTYuMzggNi4yNSAyMi42MyAwbDkwLjUxLTkwLjUxYzYuMjMtNi4yNCA2LjIzLTE2LjM3LS4wMi0yMi42MnptLTI4Ni43Mi0xNS4yYy0zLjctMy43LTYuODQtNy43OS05Ljg1LTExLjk1TDE5LjY0IDQwNC45NmMtMjUuNTcgMjMuODgtMjYuMjYgNjQuMTktMS41MyA4OC45M3M2NS4wNSAyNC4wNSA4OC45My0xLjUzbDIzOC4xMy0yNTUuMDdjLTMuOTYtMi45MS03LjktNS44Ny0xMS40NC05LjQxbC00OS4xNC00OS4xNHoiLz48L3N2Zz4="></a>

Lo ideal es construirlos por lotes de cinco — los pedidos mínimos de JLCPCB hacen de esta la cantidad mas conveniente. Únete a amigos o contacta a [L'Atelier du bandonéon libre](https://github.com/bandolibre) para expresar interés en una construcción colectiva.

A partir de cincuenta unidades, la fabricación de PCB y los interruptores Gateron — las dos partidas más importantes — el costo baja a la mitad nuevamente.

- **Firmware:** ver [`code/`](code/) — compilar con `just build` y `just flash` para flashear via ST-LINK
- **Actualización de firmware:** pon la placa en modo DFU y aparecerá como una unidad de almacenamiento masivo USB — arrastra el archivo `.uf2` sobre ella y reiniciará con el nuevo firmware, sin necesidad de programador

---

## Herramienta de configuración

La herramienta de configuración es una página web que muestra y modifica los parámetros del instrumento por USB-MIDI — funciona en ordenador, tableta o teléfono. Permite ajustar el fuelle arrastrando las curvas de empuje y tracción (o eligiendo un ajuste predefinido) para dar forma a cómo el esfuerzo se traduce en expresión, y calibrar la posición de reposo. Los cambios se aplican al instante, así que puedes ajustar el tacto mientras tocas. También permite calibrar los pedales, cambiar la afinación, la sensibilidad y el modo mesa, editar todas las propiedades del firmware, y muestra ambos teclados en vivo mientras tocas.

<a href="https://bandolibre.github.io/tools/midi.html"><img width="505" alt="Abrir la herramienta de configuración" src="https://img.shields.io/badge/Abrir_la_herramienta_de_configuraci%C3%B3n-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTQ5NiAzODRIMTYwdi0xNmMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZjLTguOCAwLTE2IDcuMi0xNiAxNnYzMmMwIDguOCA3LjIgMTYgMTYgMTZoODB2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMzM2YzguOCAwIDE2LTcuMiAxNi0xNnYtMzJjMC04LjgtNy4yLTE2LTE2LTE2em0wLTE2MGgtODB2LTE2YzAtOC44LTcuMi0xNi0xNi0xNmgtMzJjLTguOCAwLTE2IDcuMi0xNiAxNnYxNkgxNmMtOC44IDAtMTYgNy4yLTE2IDE2djMyYzAgOC44IDcuMiAxNiAxNiAxNmgzMzZ2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoODBjOC44IDAgMTYtNy4yIDE2LTE2di0zMmMwLTguOC03LjItMTYtMTYtMTZ6bTAtMTYwSDI4OFY0OGMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZDNy4yIDY0IDAgNzEuMiAwIDgwdjMyYzAgOC44IDcuMiAxNiAxNiAxNmgyMDh2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMjA4YzguOCAwIDE2LTcuMiAxNi0xNlY4MGMwLTguOC03LjItMTYtMTYtMTZ6Ii8+PC9zdmc+"></a>

<p>
  <img src="documentation/images/configuration_tool.webp" alt="Herramienta de configuración, curva del fuelle" width="49%">
  <img src="documentation/images/configuration_tool_keyboard.webp" alt="Herramienta de configuración, teclados en vivo" width="49%">
</p>

---

## Diseño

![overview](documentation/images/3dmodel_overview.webp)

Las piezas mecánicas están modeladas en Onshape — [el modelo 3D completo](https://cad.onshape.com/documents/313e70e978bf056a8dd7d76c/v/5c5fbc4088ac379c1bd1b53a/e/c6a89cb028bdc195ff70596f?showReturnToWorkspaceLink=tru) es público e interactivo. Se utilizó [fotografía de referencia](keyboard_picture/) de instrumentos reales para reproducir con precisión la forma, la disposición de teclas y la inclinación del teclado de ambas manos.
![handle_layout](documentation/images/3dmodel_handle.webp)
![keyboard_layout](documentation/images/3dmodel_keyboard_layout.webp)

Los PCB están diseñados con EasyEDA.
![3d_pcb](documentation/images/pcb_main_board_3d.png)

El fuelle es reemplazado por una **lámina de resorte equipada con dos sensores de efecto Hall** que leen su flexión. Las soluciones basadas en celdas de carga fueron descartadas — demasiado rígidas, eliminan el feedback táctil que los bandoneonistas necesitan para sentir y modular su esfuerzo — es como presionar contra una pared. La lámina resorte preserva ese feedback propioceptivo siendo a la vez simple y duradera. El grosor de la lámina puede elegirse para ajustar la rigidez del instrumento, de suave a firme.

![lámina resorte](documentation/images/bandolibre_blade.webp)

Un botón selector de sensibilidad permite selectionar entre tres niveles de amplificación para ajustar cuánto recorrido de fuelle se necesita para alcanzar la máxima expresión — útil para tocar suave.

Para un desglose detallado del comportamiento del firmware y los controles, ver [`documentation/features.md`](documentation/features.md).

---

## El sonido

Lo que mejor funciona hasta ahora son los instrumentos de simulación de la familia **SWAM** de [Audio Modeling](https://audiomodeling.com/): están basados en modelado físico en lugar de muestras, así que el CC#11 controla el modelo mismo de forma continua. La intensidad del fuelle se traduce en matices reales — el timbre cambia con la presión, las notas crecen y se apagan bajo el fuelle — en vez de un simple desvanecimiento de volumen sobre una muestra fija. [Native Instruments Session Strings](https://www.native-instruments.com/en/products/komplete/cinematic/session-strings-2) también da muy buenos resultados.

Curiosamente, las bibliotecas modernas de bandoneón nos funcionan mal. O bien suponen una disposición de teclado equivocada — mapeos cromáticos o de acordeón, sin distinción entre empuje y tracción — o bien ofrecen un soporte superficial del CC#11, con los matices fijados en muestras disparadas por velocidad que el fuelle ya no puede remodelar una vez iniciada la nota.

Para practicar — y sobre todo en el teléfono — basta con un simple soundfont de bandoneón: [European Bandoneon V2.5 de Jörg Bleymehl](https://musical-artifacts.com/artifacts/1862) da buenos resultados. Se carga en cualquier reproductor compatible con SF2, casi no consume CPU, y convierte un teléfono o una tableta en un instrumento de práctica utilizable, sin computadora ni anfitrión de plugins.

Por eso estamos buscando activamente instrumentos virtuales con soporte expresivo profundo — rica polifonía y CC#11 como controlador principal de articulación. Si conoces alguno, o quieres ayudar a construir algo hecho a medida para el bandoneón, las sugerencias y contribuciones son mas que bienvenidas.

---

## ¿Cómo queda?

<p>
  <img src="documentation/images/bandolibre_overview.webp" alt="Bandolibre overview" width="49%">
  <img src="documentation/images/bandolibre_main_module.webp" alt="Bandolibre main module" width="49%">
</p>

Cinco unidades fueron construidas y están funcionando. El firmware maneja las 142 teclas, el fuelle empuje/tracción, los pedales y la salida MIDI de forma confiable. Estos cinco instrumentos están actualmente prestados a profesores de bandoneón que nos dan retroalimentación práctica mientras pulimos el software.

Los modelos 3D y los diseños de PCB son sólidos — no hay revisiones planificadas. El foco esta puesto ahora en afinar la simulación del fuelle: lograr que el modelo de inercia haga que las notas cortas se sientan como en el instrumento real, no como un sensor.

Hay mucho por explorar respecto del software. Dado que los sensores de efecto Hall miden la posición de las teclas de forma continua — no solo encendido/apagado — el firmware tiene acceso al recorrido completo de cada tecla en todo momento. Esto abre la puerta al **MPE (MIDI Polyphonic Expression)**: curvas de presión, deslizamiento y levantamiento por nota, de forma independiente para cada una de las 142 teclas simultáneamente.

---

## ¿Cómo obtener un dispositivo?

Bandolibre sigue siendo un proyecto DIY: los planos son abiertos y cualquiera puede construir uno. Estamos puliendo el software y recopilando comentarios sobre el hardware.

Lo que falta es el marco administrativo que le permita a la asociación concretar una transacción — ceder placas, un kit o un instrumento terminado a quien lo pida.

Si quieres que te avisemos cuando sea posible:

<a href="https://forms.gle/amgxEX4XTy9Jfd538"><img width="210" alt="Mostrar interés" src="https://img.shields.io/badge/Mostrar_inter%C3%A9s-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzODQgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTMzNiA2NGgtODBjMC0zNS4zLTI4LjctNjQtNjQtNjRzLTY0IDI4LjctNjQgNjRINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MzUyYzAgMjYuNSAyMS41IDQ4IDQ4IDQ4aDI4OGMyNi41IDAgNDgtMjEuNSA0OC00OFYxMTJjMC0yNi41LTIxLjUtNDgtNDgtNDh6TTk2IDQyNGMtMTMuMyAwLTI0LTEwLjctMjQtMjRzMTAuNy0yNCAyNC0yNCAyNCAxMC43IDI0IDI0LTEwLjcgMjQtMjQgMjR6bTAtOTZjLTEzLjMgMC0yNC0xMC43LTI0LTI0czEwLjctMjQgMjQtMjQgMjQgMTAuNyAyNCAyNC0xMC43IDI0LTI0IDI0em0wLTk2Yy0xMy4zIDAtMjQtMTAuNy0yNC0yNHMxMC43LTI0IDI0LTI0IDI0IDEwLjcgMjQgMjQtMTAuNyAyNC0yNCAyNHptOTYtMTkyYzEzLjMgMCAyNCAxMC43IDI0IDI0cy0xMC43IDI0LTI0IDI0LTI0LTEwLjctMjQtMjQgMTAuNy0yNCAyNC0yNHptMTI4IDM2OGMwIDQuNC0zLjYgOC04IDhIMTY4Yy00LjQgMC04LTMuNi04LTh2LTE2YzAtNC40IDMuNi04IDgtOGgxNDRjNC40IDAgOCAzLjYgOCA4djE2em0wLTk2YzAgNC40LTMuNiA4LTggOEgxNjhjLTQuNCAwLTgtMy42LTgtOHYtMTZjMC00LjQgMy42LTggOC04aDE0NGM0LjQgMCA4IDMuNiA4IDh2MTZ6bTAtOTZjMCA0LjQtMy42IDgtOCA4SDE2OGMtNC40IDAtOC0zLjYtOC04di0xNmMwLTQuNCAzLjYtOCA4LThoMTQ0YzQuNCAwIDggMy42IDggOHYxNnoiLz48L3N2Zz4="></a>

También nos ayuda a ver cuánta gente está interesada y para qué lo tocarían. Tus respuestas quedan en la asociación y solo se usan para contactarte a propósito de Bandolibre.

---

## Comunidad

¿Preguntas, ideas, o simplemente tienes curiosidad?
Únete a [L'Atelier du bandonéon libre](https://bandolibre.github.io).

La placa principal se comunica digitalmente con las placas wing y puede soportar cualquier disposición. Es posible diseñar un nuevo teclado para un sistema diferente — Rheinische Lage, Club, Einheitsbandoneon, Peguri, Manouri — y reutilizar la placa principal.

¿Estás trabajando en algo similar? Cuéntale a la asociación — nos encantaría conectar.

<a href="mailto:bandolibre@googlegroups.com"><img width="329" alt="Escribir a la asociación" src="https://img.shields.io/badge/Escribir_a_la_asociaci%C3%B3n-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTUwMi4zIDE5MC44YzMuOS0zLjEgOS43LS4yIDkuNyA0LjdWNDAwYzAgMjYuNS0yMS41IDQ4LTQ4IDQ4SDQ4Yy0yNi41IDAtNDgtMjEuNS00OC00OFYxOTUuNmMwLTUgNS43LTcuOCA5LjctNC43IDIyLjQgMTcuNCA1Mi4xIDM5LjUgMTU0LjEgMTEzLjYgMjEuMSAxNS40IDU2LjcgNDcuOCA5Mi4yIDQ3LjYgMzUuNy4zIDcyLTMyLjggOTIuMy00Ny42IDEwMi03NC4xIDEzMS42LTk2LjMgMTU0LTExMy43ek0yNTYgMzIwYzIzLjIuNCA1Ni42LTI5LjIgNzMuNC00MS40IDEzMi43LTk2LjMgMTQyLjgtMTA0LjcgMTczLjQtMTI4LjcgNS44LTQuNSA5LjItMTEuNSA5LjItMTguOXYtMTljMC0yNi41LTIxLjUtNDgtNDgtNDhINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MTljMCA3LjQgMy40IDE0LjMgOS4yIDE4LjkgMzAuNiAyMy45IDQwLjcgMzIuNCAxNzMuNCAxMjguNyAxNi44IDEyLjIgNTAuMiA0MS44IDczLjQgNDEuNHoiLz48L3N2Zz4="></a>

---

## Lecturas adicionales

- [Otros proyectos de bandoneón electrónico](documentation/other-projects.md)

---

## Licencia

[![CC BY-NC-SA 4.0](https://mirrors.creativecommons.org/presskit/buttons/88x31/svg/by-nc-sa.eu.svg)](LICENSE.md)

Libre de construir, modificar y compartir para uso no comercial.
