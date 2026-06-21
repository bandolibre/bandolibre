---
title: Bandonéo
layout: single
permalink: /bandoneo
hidden: true
header:
  overlay_color: "#5e616c"
  overlay_image: assets/images/readme_tilted.webp
excerpt: Un instrument MIDI pour les bandonéonistes
comments: true
author_profile: true
# toc: true
# toc_label: "Sur cette page"
# toc_icon: "columns"
---

Le **Bandonéo** est un contrôleur MIDI conçu pour les passionnés, qui recrée
fidèlement la disposition du mythique bandonéon argentin et son clavier
"Rheinische Lage" à 142 voies.

<figure>
  <img src="{{ '/assets/images/bandoneo-feature.webp' | relative_url }}" alt="Rendu 3d du Bandonéo">
</figure>

Grâce à ses capteurs magnétiques à **effet Hall**, la position de chaque touche
est mesurée **1000 fois par seconde**, offrant une précision et une réactivité
exceptionnelles, sans aucune usure mécanique.

Il se connecte en **USB-MIDI** (connecteur Type-B), directement alimenté
par le port USB — aucune batterie, aucun adaptateur, aucun câble supplémentaire.

## Une Philosophie Ouverte et Communautaire

1. **Une Conception Libre** <a target="_blank" href="https://creativecommons.org/licenses/by-nc-sa/4.0/"><img style="float: right;" src="https://mirrors.creativecommons.org/presskit/buttons/88x31/svg/by-nc-sa.eu.svg"></a>

   Le Bandonéo est un projet entièrement open-source. Tous les plans des pièces
   mécaniques et des circuits électroniques sont publiés sous licence
   [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/){:target="_blank"}.

   Cela signifie que chacun est libre de le construire, de le réparer, de le
   modifier et de partager ses améliorations avec la communauté (dans un cadre
   non commercial).

   Tous les fichiers sources — schémas, PCB, firmware, mécanique — sont
   disponibles sur [GitHub](https://github.com/piwicode/bandoneo){:target="_blank"}.

2. **Une Plateforme Modulaire**

   Ne vous contentez pas d'un simple instrument, appropriez-vous-le !

   La conception modulaire vous invite à expérimenter : personnalisez les
   claviers, ajoutez de nouveaux capteurs, ou reprogrammez entièrement le
   logiciel pour créer l'instrument de vos rêves. Les possibilités sont
   infinies.

## Comment Obtenir Votre Bandonéo ?

Le Bandonéo n'est pas un produit commercial. C'est une aventure communautaire.
Pour obtenir le vôtre, deux chemins s'offrent à vous :

- **La Fabrication Autonome (DIY)**

  Vous avez l'âme d'un maker ? Téléchargez les fichiers, imprimez les pièces en
  3D (dans un FabLab près de chez vous, par exemple) et fabriquez vous-même les
  circuits. Vous avez le contrôle total sur chaque composant.

- **Le Kit de Développement**

  Pour vous lancer plus facilement, [l'Atelier du Bandonéon Libre](/)
  organise des achats groupés de kits de développement. Ce kit est un Bandonéo
  MIDI complet et fonctionnel, livré partiellement assemblé avec toutes les
  pièces mécaniques, électroniques et la visserie nécessaire.
  
  C'est le moyen idéal pour commencer à jouer rapidement tout en gardant la
  possibilité de le modifier, grâce notamment aux connecteurs de reprogrammation
  ST-LINK présents sur les cartes. Vos retours d'expérience sur ce kit seront
  précieux pour l'avenir du projet.

## Characteristiques Techniques

- **Clavier :** Rheinische Lage 142 voies
- **Capteurs :** Technologie à effet Hall, sans contact mécanique
- **Précision :** Mesure de la position des touches 1000 fois par seconde
- **Soufflet :** Lame flexion + 2 capteurs Hall → MIDI CC#11 Expression
- **Entrées expression :** 2 × jack TRS 6.35 mm (CC#1 Modulation, CC#4 Foot Controller)
- **Connectique :** USB-MIDI (connecteur Type-B, alimentation bus USB)
- **Microcontrôleur :** STM32G474CBT6 (identique sur les 3 cartes)
- **Firmware :** Open-source, reprogrammable via connecteur ST-LINK

## État d'Avancement du Projet

- ✅ **Mécanique** — Conception finalisée.
  [Explorez le modèle 3D complet et interactif sur Onshape](https://cad.onshape.com/documents/313e70e978bf056a8dd7d76c/w/44f56bd9f99346f07c3c93d4/e/c6a89cb028bdc195ff70596f?renderMode=0&uiState=68dae2d4084543739eda2bc5){:target="_blank"}
- ✅ **Électronique** — Cartes conçues et fabriquées (carte mère + clavier gauche + clavier droit).
  Le premier Bandonéo est terminé.
- ⏳ **Assemblage** — Je m'apprête à assembler cinq exemplaires pour les prêter à des
  professeurs de bandonéon et recueillir des retours terrain.
- ⏳ **Firmware** — Les touches et l'USB-MIDI fonctionnent. Le travail en cours porte sur
  l'affinage du modèle d'inertie du soufflet, afin que les notes courtes sonnent naturellement,
  comme sur un instrument acoustique.

Retrouvez tous les détails techniques, le code source et les fichiers de fabrication sur
[github.com/piwicode/bandoneo](https://github.com/piwicode/bandoneo){:target="_blank"}.

## Rejoignez la Communauté !

Vous avez des questions, des idées ?<br/>
rejoignez [l'Atelier du Badonéon Libre](/)

