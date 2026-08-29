# Changelog

## v0.8

**Talking to each other.** Speech to text, entirely on the device — press the
balloon, a bubble appears over your character, it fills in as you speak, and a
pause of a second and a half sends it. Recognition is Vosk with a local model:
no speech service, no account, nothing recorded ever leaves the machine, and it
works with the router unplugged. The game mutes its own music and sound effects
while the microphone is open, and asks the platform for noise suppression and
echo cancellation.

**A megaphone button.** Shouting to the channel or the world is now a button
rather than an item you have to buy. It sends the real megaphone packet and the
server runs the code it always did, so the artwork, the name prefix and the
level rules are the same ones the item used.

### Fixed

- **The screen was never fullscreen.** The game rendered a full 1920x1080 frame
  into a slot 55 pixels shorter, pushed down by the status bar's reserved strip
  — so the top was a black band and the HP bar fell off the bottom. This is the
  "cut off screen" that had survived a rotation fix, a stale-drawable fix, a
  letterbox and three resolution changes; none of them was the cause.
- **196 hats could not be drawn**, 60 of them on sale. Two separate faults: the
  cap type was matched against three literal strings and everything else became
  "draw nothing" (144 hats), and the layer a hat asked for was only drawn in one
  branch out of four, so 27 more hid your hair and drew nothing in its place.
- **The map kept the last map's monsters.** Changing character out of the Cygnus
  tutorial put four Tutorial Tinos on Maple Road. All eight map containers
  emptied their live objects on a map change but not their pending spawn queues.
- **Every server message was read and thrown away** — megaphones, notices,
  announcements. Only the scrolling banner was ever shown.
- **Quest EXP was never reported**, and misparsed: the client read a fixed
  layout where the server writes an extra byte, then printed "not handled" in
  red. Finishing a quest levelled you up with nothing to say why.
- **Every "Weekdays" 2x EXP card was inert** at every hour of every day — nine
  rows in the coupon table had lost the leading digit of their item id. Coupons
  are no longer restricted to a weekday and a four-hour window.
- **NPC menus ran off the screen.** A long list of choices was drawn past the
  dialogue box, over the minimap and off the bottom edge; the box was sized from
  the message and never counted the choices. Now sized, clipped and scrollable.
- **Nothing scrolled.** `Slider::send_scroll` had always existed and no window
  ever called it. The inventory, skill book, quest log, NPC dialogue and cash
  shop now take the wheel — and a thumbstick.
- **The cash shop character could not move.** Walking, jumping and the stances
  were all implemented and had never been handed a key. He also faced backwards
  and stood shin-deep in the floor.
- **A megaphone needed level 10** — Nexon's anti-spam rule for a game where they
  cost real money, which only stopped a new character talking.

### Changed

- The client is no longer pinned to four screen resolutions. The status bar's
  position was written four different ways that all meant "120 from the bottom",
  and the login and character screens each hardcoded 800x600 twice over.
- Status bar: Event and Community retired, the HP/MP panel moved left, and the
  remaining buttons drawn larger for a thumb rather than a mouse.
- Cash shop: **MY CASH ITEMS** replaced with an **INFORMATION** panel showing
  what the selected item actually does, and buying now asks first, naming the
  item and the price.
- The thumbstick scrolls; walking is on the d-pad.

### Known

- The minimap cannot be sharpened by enlarging it. There is one set of frame
  artwork and one bitmap per map in the game data — no higher-resolution source
  exists to draw from.
- Quest 3 controllers still reach a 2D app as six signals, not as a gamepad.
  See `docs_QUEST.md`; the investigation is finished and should not be repeated.
