# Touch HUD design

Design reference for the in-race touch controls of SuperTuxTouch (`RaceGUIMultitouch`
+ `MultitouchDevice`). Written before the v2 "thumb arc" rework so layout, theming and
input behaviour stay consistent as the HUD evolves.

Target device class: Linux tablets held in landscape with two hands, thumbs resting on
the lower-left and lower-right corners. Reference hardware: Surface Pro (2880x1920,
~267 DPI, 3:2).

## 1. What the game actually needs

Every action a player can perform during a race, ranked by how often a competent player
uses it. This ranking drives size and placement.

| Rank | Action | `PlayerAction` | Notes on the real mechanic |
|---|---|---|---|
| 1 | Steer left/right | `PA_STEER_LEFT` / `PA_STEER_RIGHT` | Continuous, analogue. Never stops during a race. |
| 2 | Accelerate | `PA_ACCEL` | Auto-acceleration is on by default, so this is normally implicit. |
| 3 | Drift | `PA_DRIFT` | Held. Requires simultaneous steering and speed above `getSkidMinSpeed()`; releasing after enough accumulated time grants a speed boost. The highest-skill action in the game. |
| 4 | Fire item | `PA_FIRE` | Edge-triggered. Only meaningful while holding a powerup. |
| 5 | Nitro | `PA_NITRO` | Only works while accelerating and while `getEnergy() > 0`. |
| 6 | Brake / reverse | `PA_BRAKE` | Brakes while moving forward; becomes reverse once stopped. |
| 7 | Look back | `PA_LOOK_BACK` | Rear camera, and also makes cake / bowling / plunger / bubblegum fire backwards. Used *together with* fire. |
| 8 | Rescue | `PA_RESCUE` | Recovery when stuck or off-track. Costly if pressed by accident. |
| 9 | Pause | escape | Rare, deliberate. |

There is no in-race camera-change action, and no separate "fire backwards" action, so the
control set is complete with **one stick and six buttons**. The problem to solve is not
missing functionality — it is placement, feedback and consistency.

### Derived requirements

- Drift, fire and nitro must be pressable **without breaking steering**, i.e. one per
  thumb region, right thumb able to slide between them (upstream already supports this
  via "linked buttons": `BUTTON_FIRE`…`BUTTON_LOOK_BACKWARDS`).
- Look back must sit next to fire, because the two are used as a pair (Law of Proximity).
- Rescue must be reachable but must **not** be adjacent to a primary button, and must not
  be part of the linked-slide group — an accidental rescue costs a race.
- Nitro and fire have an availability state (energy / powerup). The state must be
  visible, and the button must **never disappear**, or the player loses the muscle memory
  for every other button in the cluster.

## 2. Ergonomic model

Both thumbs pivot near the bottom corners of the device. Buttons at a constant radius
from that pivot cost the same effort to reach; the comfortable neutral rotation is the
diagonal. This gives a **radial arc**, not a grid.

```
                                                  ·  rescue      (outer arc, 112.5 deg)
                                          ·  nitro                (inner arc,  90 deg)
                                  ·  look back                    (outer arc, 157.5 deg)
                          ·  DRIFT                                (inner arc, 135 deg)
     ( steering region )       ·  fire                            (inner arc, 180 deg)
                                                       P  <- thumb pivot
```

Angles are measured from the pivot `P`, anticlockwise from "towards the screen centre".
The inner arc holds the three highest-frequency actions; the outer arc holds the two
lower-frequency ones and is a deliberate stretch.

Left thumb gets a **dynamic stick**: the steering region is large and invisible, and the
stick graphic materialises under the thumb wherever it lands. Steering is measured
relative to that landing point, not relative to a fixed screen position. A fixed stick
forces the player to look down and to land precisely on centre, and landing off-centre
applies instant full lock.

## 3. Layout tokens

All sizes derive from screen height so the HUD is resolution independent. `U = height *
m_multitouch_scale`.

| Token | Value | Purpose |
|---|---|---|
| `btn` | `0.115 * U` | Primary button diameter |
| `margin` | `0.05 * U` | Safe margin from screen edges |
| `gap` | `0.18 * btn` | Minimum inactive space between neighbours |
| `R1` | `1.75 * btn` | Inner arc radius |
| `R2` | `R1 + 1.10 * btn` | Outer arc radius |
| `stick_visual` | `0.30 * U` | Diameter of the drawn stick base |
| `stick_region` | left `0.44 * width`, bottom `0.66 * height` | Invisible steering hit region |

Button sizes, expressed as multiples of `btn`:

| Button | Size | Arc | Angle |
|---|---|---|---|
| Drift | 1.15 | inner | 135 deg |
| Fire / item | 1.00 | inner | 180 deg |
| Nitro | 1.00 | inner | 90 deg |
| Look back | 0.72 | outer | 157.5 deg |
| Rescue | 0.72 | outer | 112.5 deg |
| Pause | 0.62 | top strip | — |

Pivot: `P = (width - margin - btn/2, height - margin - btn/2)`, mirrored horizontally when
`m_multitouch_inverted` is set. `R1 = 1.75 * btn` is the smallest radius that keeps the
`gap` between the 1.15 and 1.00 sized neighbours at 45 degree spacing.

Pause stays in the top strip, offset to the right of the kart-position icon column that
`drawGlobalPlayerIcons` owns, and is drawn at reduced opacity so it never competes with
the race for attention.

### Hit area versus visual area

The hit rectangle registered with `MultitouchDevice` is the source of truth. The plate is
drawn **at or inside** that rectangle, never outside it. The previous implementation drew
each plate 40% larger than its hitbox, so taps on the visible edge of a button did
nothing. Where extra forgiveness is wanted, the hitbox grows and the art stays put.

## 4. Visual system

One plate geometry, one glass material, one accent hue per function. Only the hue and the
icon change between buttons, so the cluster reads as a single group (Law of Similarity)
while each button stays individually identifiable (Von Restorff).

| Element | Accent | Rationale |
|---|---|---|
| Drift | amber `#FFB020` | Heat / tyre smoke; also the "primary" of the cluster |
| Fire / item | cyan `#35C8FF` | Neutral-cool, does not clash with powerup icon art |
| Nitro | green `#5CE86A` | Matches the in-game nitro can and gauge |
| Look back | slate `#9FB3C8` | Passive, informational |
| Rescue | red `#FF6B5B` | Warns that this is a costly action |
| Pause | slate, low alpha | Recedes |
| Stick | white / slate | Neutral so track colours read through it |

Every plate shares: circular glass fill at 34% alpha over a dark base, a 6% ring in the
accent hue, and one top-left specular highlight. Pressed state keeps the accent and adds
a brighter fill and ring — pressing must never change a button's colour identity.

States:

- **Available** — full accent, full-opacity icon.
- **Unavailable** (no powerup, no nitro) — plate stays at reduced alpha, icon dimmed.
  The button never disappears.
- **Pressed** — brighter fill and ring.

## 5. Feedback the HUD must show

| Signal | How |
|---|---|
| Steering amount | Knob offset from the stick centre, clamped to the base |
| Accelerating | Green arc on the upper half of the stick base, alpha proportional to accel |
| Braking | Red arc on the lower half of the stick base |
| Nitro remaining | The nitro plate fills bottom-up with the bright accent, proportional to `getEnergy() / getNitroMax()` |
| Item count | Digit on the fire plate when `getNum() > 1` |
| Item present | Powerup icon on the fire plate; dimmed placeholder when empty |

The upstream energy meter is drawn beside the nitro button using the desktop speedometer
gauge art, at a fixed pixel offset that assumes the old grid layout. Replacing it with a
fill on the nitro plate itself removes the offset dependency, keeps nitro state where the
player's thumb already is, and keeps the glass theme consistent.

## 6. Input behaviour

- **Dynamic stick origin.** On the touch-down that starts a steering gesture, record the
  contact point as the origin. Axes are `(event - origin) / (visual_radius)`, clamped to
  `[-1, 1]`. On release the origin resets and the base animates back to its home position.
- **Axis clamping.** Axes were previously unclamped, so dragging beyond the widget sent
  the drawn knob off the base. Clamp on read.
- **Tracking outside the widget.** Already correct upstream: once a finger owns the
  steering widget it keeps updating the axes even outside the rectangle.
- **Deadzone and sensitivity** stay user-configurable; defaults unchanged.

## 7. Principles applied

- *Fitts's law* — the most used control (drift) is the largest and sits at the neutral
  thumb rotation; the rarest (pause) is smallest and furthest.
- *Hick's law* — six buttons, no more. Every action already has a home; nothing is added.
- *Law of proximity / common region* — one arc per thumb, with a clear gap between the
  inner and outer arcs so the two priority tiers read as separate groups.
- *Law of similarity* — a single plate geometry and material across the whole cluster.
- *Jakob's law* — steering left, actions right, pause in the top strip; the arrangement
  most mobile racers use.
- *Consistency* — buttons never move, resize or vanish based on game state; only their
  colour, fill and icon opacity change.
- *Von Restorff* — rescue is the only red control on screen.
