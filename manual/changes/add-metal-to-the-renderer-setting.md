---
title: Add Metal to the Renderer setting
category: feature
release: 0.2.0
targets:
- type: key
  id: Renderer
  effect: changed
credit: [kanechen]
---

`Renderer` in the `[Video]` section of `sun.ini` accepts `5` for Metal, the graphics interface macOS uses. The existing values are unchanged, and a value outside the accepted range is still treated as `0`.

Choosing an interface the machine cannot provide remains a startup failure rather than a fallback, so `5` only starts the game where Metal is available. Metal is not reachable on the supported Windows build; it is offered so that a configuration file can name the interface a future build uses without a new value being added later.
