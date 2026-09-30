![Bandolibre keyboard](documentation/images/3dmodel_keyboard_tilt.webp)

🇬🇧 [English](README.md) | 🇫🇷 **Français** | 🇪🇸 [Español](README.es.md)

# Bandolibre

Un contrôleur MIDI open-source qui fait entrer le bandonéon argentin dans l'ère numérique.

**Bandolibre n'a qu'un seul objectif :** convertir en MIDI les pressions sur les touches et les mouvements du soufflet — fidèlement, instantanément, sans les contraintes de coût ou de bruit d'un instrument acoustique. Branchez-le sur un ordinateur, une tablette ou un téléphone via USB et vous êtes prêt à jouer, composer ou vous entraîner.

Une initiative ouverte de [L'Atelier du bandonéon libre](https://github.com/bandolibre)

---

## À l'écoute

[![Démo du Bandolibre avec une clarinette virtuelle](https://img.youtube.com/vi/6s1wlRKlAk4/maxresdefault.jpg)](https://youtu.be/6s1wlRKlAk4)

Voir les démos : [avec une clarinette virtuelle](https://youtu.be/6s1wlRKlAk4) · [avec des instruments virtuels à cordes](https://youtu.be/nJ0j7DtbYDk).

---

## Pour qui ?

- **Les élèves** qui n'ont pas encore d'instrument et veulent commencer à apprendre
- **Les musiciens** qui veulent s'entraîner en silence — à la maison, en voyage, à toute heure
- **Les compositeurs** qui saisissent des partitions note par note dans un logiciel de notation
- **Les musiciens de scène** qui pilotent synthétiseurs et sampleurs en concert ou en studio

---

## Comment ça fonctionne

- **Clavier Rheinische Lage à 142 tons** — les deux mains, doigtés exacts du bandonéon argentin
- **Capteurs à effet Hall sur chaque touche** — pas de contact mécanique, aucune usure, 2400 lectures par seconde
- **Lame-ressort + capteur** pour le soufflet — mesure l'effort de poussée et de traction et émet MIDI CC#11 (Expression), comme le vrai instrument
- **Deux entrées pédale d'expression 6,35 mm** — compatibles M-Audio EX-P ; pédale 1 envoie CC#1 (Modulation), pédale 2 envoie CC#4 (Contrôleur de pied)
- **USB-MIDI** nativement — branchez sur n'importe quel DAW, logiciel de notation ou synthé ; aucun pilote nécessaire

---

## Fonctionnalités

**Accord du clavier** — un bouton pour selecionne la dispositions de clavier parmis: Rheinische Tonlage (bisonore, 142 tons), Peguri ou Manoury.

**Mode table** — un bouton active le mode table : les touches se déclenchent immédiatement à vélocité fixe, sans mouvement de soufflet. Idéal pour saisir une partition note par note sans actionner le soufflet.

**Jouez partout** — Bandolibre est alimenté par le bus USB ; n'importe quel téléphone, tablette ou ordinateur portable avec un synthé logiciel devient le moteur sonore. Un petit hub USB avec prise casque et pass-through d'alimentation vous donne audio, charge et MIDI en un seul câble — testé et tient dans une poche.

**Adaptable** — le standard USB-MIDI est compatible avec tout l'écosystème d'adaptateurs : branchez un adaptateur USB Bluetooth MIDI pour jouer sans fil, ou un adaptateur USB-vers-DIN-5 pour piloter des synthétiseurs matériels vintage.

<a href="documentation/user_manual.fr.md"><img width="290" alt="Manuel d'utilisation" src="https://img.shields.io/badge/Manuel_d%27utilisation-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1NzYgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTU0Mi4yMiAzMi4wNWMtNTQuOCAzLjExLTE2My43MiAxNC40My0yMzAuOTYgNTUuNTktNC42NCAyLjg0LTcuMjcgNy44OS03LjI3IDEzLjE3djM2My44N2MwIDExLjU1IDEyLjYzIDE4Ljg1IDIzLjI4IDEzLjQ5IDY5LjE4LTM0LjgyIDE2OS4yMy00NC4zMiAyMTguNy00Ni45MiAxNi44OS0uODkgMzAuMDItMTQuNDMgMzAuMDItMzAuNjZWNjIuNzVjLjAxLTE3LjcxLTE1LjM1LTMxLjc0LTMzLjc3LTMwLjd6TTI2NC43MyA4Ny42NEMxOTcuNSA0Ni40OCA4OC41OCAzNS4xNyAzMy43OCAzMi4wNSAxNS4zNiAzMS4wMSAwIDQ1LjA0IDAgNjIuNzVWNDAwLjZjMCAxNi4yNCAxMy4xMyAyOS43OCAzMC4wMiAzMC42NiA0OS40OSAyLjYgMTQ5LjU5IDEyLjExIDIxOC43NyA0Ni45NSAxMC42MiA1LjM1IDIzLjIxLTEuOTQgMjMuMjEtMTMuNDZWMTAwLjYzYzAtNS4yOS0yLjYyLTEwLjE0LTcuMjctMTIuOTl6Ii8+PC9zdmc+"></a>

---

## Tout le monde peut le construire

C'est un projet DIY entièrement ouvert. Les PCB sont conçus pour la fabrication économique JLCPCB deux couches — abordables et faciles à commander. Les pièces mécaniques sont imprimables en 3D (un FabLab près de chez vous convient parfaitement). Le firmware est open-source et peut être flashé avec une sonde [ST-LINK](https://www.st.com/en/development-tools/stlink-v3minie.html) standard et un câble [TC-2070-IDC-050](https://www.tag-connect.com/product/tc2070-idc-050).

Construire un Bandolibre coûte à peu près autant qu'un bon clavier MIDI ou une bonne paire de casques de studio comme le DT-770 Pro.

La nomenclature, l'outillage, les consommables et le montage pas à pas se trouvent dans les instructions de montage :

<a href="documentation/assembly_instructions.md"><img width="350" alt="Instructions de montage" src="https://img.shields.io/badge/Instructions_de_montage-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1NzYgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTU3MS4zMSAxOTMuOTRsLTIyLjYzLTIyLjYzYy02LjI1LTYuMjUtMTYuMzgtNi4yNS0yMi42MyAwbC0xMS4zMSAxMS4zMS0yOC45LTI4LjljNS42My0yMS4zMS4zNi00NC45LTE2LjM1LTYxLjYxbC00NS4yNS00NS4yNWMtNjIuNDgtNjIuNDgtMTYzLjc5LTYyLjQ4LTIyNi4yOCAwbDkwLjUxIDQ1LjI1djE4Ljc1YzAgMTYuOTcgNi43NCAzMy4yNSAxOC43NSA0NS4yNWw0OS4xNCA0OS4xNGMxNi43MSAxNi43MSA0MC4zIDIxLjk4IDYxLjYxIDE2LjM1bDI4LjkgMjguOS0xMS4zMSAxMS4zMWMtNi4yNSA2LjI1LTYuMjUgMTYuMzggMCAyMi42M2wyMi42MyAyMi42M2M2LjI1IDYuMjUgMTYuMzggNi4yNSAyMi42MyAwbDkwLjUxLTkwLjUxYzYuMjMtNi4yNCA2LjIzLTE2LjM3LS4wMi0yMi42MnptLTI4Ni43Mi0xNS4yYy0zLjctMy43LTYuODQtNy43OS05Ljg1LTExLjk1TDE5LjY0IDQwNC45NmMtMjUuNTcgMjMuODgtMjYuMjYgNjQuMTktMS41MyA4OC45M3M2NS4wNSAyNC4wNSA4OC45My0xLjUzbDIzOC4xMy0yNTUuMDdjLTMuOTYtMi45MS03LjktNS44Ny0xMS40NC05LjQxbC00OS4xNC00OS4xNHoiLz48L3N2Zz4="></a>

La construction par lot de cinq est idéale — les commandes minimum JLCPCB en font l'unité naturelle. Faites équipe avec des amis ou contactez [L'Atelier du bandonéon libre](https://github.com/bandolibre) pour manifester votre intérêt pour une construction collective.

À cinquante unités, la fabrication des PCB et les interrupteurs Gateron — les deux postes les plus importants — baissent encore de moitié.

- **Firmware :** voir [`code/`](code/) — construire avec `just build` et `just flash`, flasher via ST-LINK
- **Mise à jour du firmware :** mettez la carte en mode DFU, elle apparaît comme une clé USB — déposez-y le fichier `.uf2` et elle redémarre avec le nouveau firmware, aucun programmateur nécessaire

---

## Outil de configuration

L'outil de configuration est une page web qui affiche et modifie les paramètres de l'instrument en USB-MIDI — il fonctionne sur ordinateur, tablette ou téléphone. Il permet de régler le soufflet en déplaçant les courbes de poussé et de tiré (ou en choisissant un préréglage) pour façonner la façon dont l'effort se traduit en expression, et de calibrer la position de repos. Les changements s'appliquent immédiatement : on peut ajuster le toucher tout en jouant. Il permet aussi de calibrer les pédales, de changer d'accord, de sensibilité et de mode table, de modifier toutes les propriétés du firmware, et affiche les deux claviers en direct pendant le jeu.

<a href="https://bandolibre.github.io/tools/midi.html"><img width="406" alt="Ouvrir l'outil de configuration" src="https://img.shields.io/badge/Ouvrir_l%27outil_de_configuration-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTQ5NiAzODRIMTYwdi0xNmMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZjLTguOCAwLTE2IDcuMi0xNiAxNnYzMmMwIDguOCA3LjIgMTYgMTYgMTZoODB2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMzM2YzguOCAwIDE2LTcuMiAxNi0xNnYtMzJjMC04LjgtNy4yLTE2LTE2LTE2em0wLTE2MGgtODB2LTE2YzAtOC44LTcuMi0xNi0xNi0xNmgtMzJjLTguOCAwLTE2IDcuMi0xNiAxNnYxNkgxNmMtOC44IDAtMTYgNy4yLTE2IDE2djMyYzAgOC44IDcuMiAxNiAxNiAxNmgzMzZ2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoODBjOC44IDAgMTYtNy4yIDE2LTE2di0zMmMwLTguOC03LjItMTYtMTYtMTZ6bTAtMTYwSDI4OFY0OGMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZDNy4yIDY0IDAgNzEuMiAwIDgwdjMyYzAgOC44IDcuMiAxNiAxNiAxNmgyMDh2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMjA4YzguOCAwIDE2LTcuMiAxNi0xNlY4MGMwLTguOC03LjItMTYtMTYtMTZ6Ii8+PC9zdmc+"></a>

<p>
  <img src="documentation/images/configuration_tool.webp" alt="Outil de configuration, courbe du soufflet" width="49%">
  <img src="documentation/images/configuration_tool_keyboard.webp" alt="Outil de configuration, claviers en direct" width="49%">
</p>

---

## Conception

![overview](documentation/images/3dmodel_overview.webp)

Les pièces mécaniques sont modélisées dans Onshape — [le modèle 3D complet](https://cad.onshape.com/documents/313e70e978bf056a8dd7d76c/v/5c5fbc4088ac379c1bd1b53a/e/c6a89cb028bdc195ff70596f?showReturnToWorkspaceLink=tru) est public et interactif. Des [photographies de référence](keyboard_picture/) d'instruments réels ont été utilisées pour reproduire fidèlement la forme, le placement des touches et l'inclinaison du clavier des deux mains.
![handle_layout](documentation/images/3dmodel_handle.webp)
![keyboard_layout](documentation/images/3dmodel_keyboard_layout.webp)

Les PCB sont conçus avec EasyEDA.
![3d_pcb](documentation/images/pcb_main_board_3d.png)

Le soufflet est remplacé par une **lame-ressort instrumentée de deux capteurs à effet Hall** qui lisent sa flexion. Les solutions à base de cellule de charge n'ont pas été retenues car elles sont trop rigides et n'aident pas à ressentir et moduler la pression — ça joue comme si on appuyait sur un mur. La lame-ressort préserve ce retour proprioceptif tout en étant simple et durable. L'épaisseur de la lame peut être choisie pour régler la rigidité de l'instrument, du plus souple au plus ferme.

![3d_pcb](documentation/images/bandolibre_blade.webp)

Un bouton sélecteur de sensibilité permet de cycler entre trois niveaux d'amplification pour ajuster la course de soufflet nécessaire à l'expression maximale — utile pour jouer doucement ou avec une lame plus rigide.

Pour un détail du comportement du firmware et des commandes, voir [`documentation/features.md`](documentation/features.md).

---

## Le son

Ce qui fonctionne le mieux jusqu'ici, ce sont les instruments de simulation de la gamme **SWAM** d'[Audio Modeling](https://audiomodeling.com/) : ils reposent sur une modélisation physique plutôt que sur des échantillons, et le CC#11 pilote donc le modèle lui-même en continu. L'intensité du soufflet ressort en véritables nuances — un timbre qui change avec la pression, des notes qui enflent et s'éteignent sous le soufflet — au lieu d'un simple fondu de volume sur un échantillon figé. [Native Instruments Session Strings](https://www.native-instruments.com/en/products/komplete/cinematic/session-strings-2) donne également d'excellents résultats.

Curieusement, les bibliothèques de bandonéon modernes nous conviennent mal. Soit elles supposent une disposition de clavier inadaptée — mappages chromatiques ou d'accordéon, sans distinction poussé/tiré — soit elles n'offrent qu'un support superficiel du CC#11, avec des nuances figées dans des échantillons déclenchés à la vélocité, que le soufflet ne peut plus remodeler une fois la note lancée.

Pour travailler — et en particulier sur téléphone — une simple soundfont de bandonéon suffit : [European Bandoneon V2.5 de Jörg Bleymehl](https://musical-artifacts.com/artifacts/1862) donne de bons résultats. Elle se charge dans n'importe quel lecteur SF2, ne coûte presque rien en CPU, et transforme un téléphone ou une tablette en instrument de travail utilisable, sans ordinateur portable ni hôte de plugins.

Nous cherchons donc activement des instruments virtuels avec un support expressif profond — polyphonie riche et CC#11 comme pilote d'articulation principal. Si vous en connaissez un, ou souhaitez aider à construire quelque chose de taillé pour le bandonéon, suggestions et contributions sont les bienvenues.

---

## Où en sommes-nous

<p>
  <img src="documentation/images/bandolibre_overview.webp" alt="Bandolibre overview" width="49%">
  <img src="documentation/images/bandolibre_main_module.webp" alt="Bandolibre main module" width="49%">
</p>

Cinq unités sont construites et fonctionnelles. Le firmware gère les 142 touches, le soufflet poussé/tiré, les pédales et la sortie MIDI de manière fiable. Ces instruments sont actuellement prêtés à des professeurs de bandonéon qui nous donnent leurs retours terrain pendant que nous peaufinons le logiciel.

Les modèles 3D et les conceptions de PCB sont solides — aucune révision prévue. L'attention se porte en ce moment sur le réglage de la simulation du soufflet : faire en sorte que le modèle d'inertie rende les notes courtes aussi vivantes que sur le vrai instrument.

Il reste beaucoup à explorer côté logiciel. Parce que les capteurs à effet Hall mesurent en continu la position des touches — pas seulement ouvert/fermé — le firmware a accès à la course complète de chaque touche à tout moment. Cela ouvre la voie au **MPE (MIDI Polyphonic Expression)** : courbes de pression, de glissé et de relâché par note, indépendamment pour chacune des 142 touches simultanément.

---

## Comment obtenir un appareil

Bandolibre reste un projet DIY : les plans sont ouverts et chacun peut en construire un. Nous affinons le logiciel et recueillons les retours sur le matériel.

Ce qui manque, c'est le cadre administratif qui permettrait à l'association de conclure une transaction — céder des cartes, un kit ou un instrument fini à quelqu'un qui le demande.

Si vous souhaitez que nous vous prévenions quand ce sera possible :

<a href="https://forms.gle/amgxEX4XTy9Jfd538"><img width="303" alt="Manifester son intérêt" src="https://img.shields.io/badge/Manifester_son_int%C3%A9r%C3%AAt-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzODQgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTMzNiA2NGgtODBjMC0zNS4zLTI4LjctNjQtNjQtNjRzLTY0IDI4LjctNjQgNjRINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MzUyYzAgMjYuNSAyMS41IDQ4IDQ4IDQ4aDI4OGMyNi41IDAgNDgtMjEuNSA0OC00OFYxMTJjMC0yNi41LTIxLjUtNDgtNDgtNDh6TTk2IDQyNGMtMTMuMyAwLTI0LTEwLjctMjQtMjRzMTAuNy0yNCAyNC0yNCAyNCAxMC43IDI0IDI0LTEwLjcgMjQtMjQgMjR6bTAtOTZjLTEzLjMgMC0yNC0xMC43LTI0LTI0czEwLjctMjQgMjQtMjQgMjQgMTAuNyAyNCAyNC0xMC43IDI0LTI0IDI0em0wLTk2Yy0xMy4zIDAtMjQtMTAuNy0yNC0yNHMxMC43LTI0IDI0LTI0IDI0IDEwLjcgMjQgMjQtMTAuNyAyNC0yNCAyNHptOTYtMTkyYzEzLjMgMCAyNCAxMC43IDI0IDI0cy0xMC43IDI0LTI0IDI0LTI0LTEwLjctMjQtMjQgMTAuNy0yNCAyNC0yNHptMTI4IDM2OGMwIDQuNC0zLjYgOC04IDhIMTY4Yy00LjQgMC04LTMuNi04LTh2LTE2YzAtNC40IDMuNi04IDgtOGgxNDRjNC40IDAgOCAzLjYgOCA4djE2em0wLTk2YzAgNC40LTMuNiA4LTggOEgxNjhjLTQuNCAwLTgtMy42LTgtOHYtMTZjMC00LjQgMy42LTggOC04aDE0NGM0LjQgMCA4IDMuNiA4IDh2MTZ6bTAtOTZjMCA0LjQtMy42IDgtOCA4SDE2OGMtNC40IDAtOC0zLjYtOC04di0xNmMwLTQuNCAzLjYtOCA4LThoMTQ0YzQuNCAwIDggMy42IDggOHYxNnoiLz48L3N2Zz4="></a>

Cela nous aide aussi à voir combien de personnes sont intéressées et ce qu'elles aimeraient en jouer. Vos réponses restent au sein de l'association et servent uniquement à vous recontacter au sujet de Bandolibre.

---

## Communauté

Questions, idées, ou simplement curieux ?
Rejoignez [L'Atelier du bandonéon libre](https://bandolibre.github.io).

La carte principale communique numériquement avec les cartes de ailes et peut prendre en charge de noeaux types de clavier. Il est possible de concevoir un nouveau clavier pour un système différent — Einheitsbandoneon, Peguri, Manouri — et de réutiliser la carte principale.

Vous travaillez sur quelque chose de similaire ? Faites-le savoir à l'association — nous serions ravis d'échanger.

<a href="mailto:bandolibre@googlegroups.com"><img width="303" alt="Écrire à l'association" src="https://img.shields.io/badge/%C3%89crire_%C3%A0_l%27association-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTUwMi4zIDE5MC44YzMuOS0zLjEgOS43LS4yIDkuNyA0LjdWNDAwYzAgMjYuNS0yMS41IDQ4LTQ4IDQ4SDQ4Yy0yNi41IDAtNDgtMjEuNS00OC00OFYxOTUuNmMwLTUgNS43LTcuOCA5LjctNC43IDIyLjQgMTcuNCA1Mi4xIDM5LjUgMTU0LjEgMTEzLjYgMjEuMSAxNS40IDU2LjcgNDcuOCA5Mi4yIDQ3LjYgMzUuNy4zIDcyLTMyLjggOTIuMy00Ny42IDEwMi03NC4xIDEzMS42LTk2LjMgMTU0LTExMy43ek0yNTYgMzIwYzIzLjIuNCA1Ni42LTI5LjIgNzMuNC00MS40IDEzMi43LTk2LjMgMTQyLjgtMTA0LjcgMTczLjQtMTI4LjcgNS44LTQuNSA5LjItMTEuNSA5LjItMTguOXYtMTljMC0yNi41LTIxLjUtNDgtNDgtNDhINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MTljMCA3LjQgMy40IDE0LjMgOS4yIDE4LjkgMzAuNiAyMy45IDQwLjcgMzIuNCAxNzMuNCAxMjguNyAxNi44IDEyLjIgNTAuMiA0MS44IDczLjQgNDEuNHoiLz48L3N2Zz4="></a>

---

## Pour aller plus loin

- [Autres projets de bandonéon électronique](documentation/other-projects.md)

---

## Licence

[![CC BY-NC-SA 4.0](https://mirrors.creativecommons.org/presskit/buttons/88x31/svg/by-nc-sa.eu.svg)](LICENSE.md)

Libre de construire, modifier et partager pour un usage non commercial.
