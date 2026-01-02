# Battery State LED Effect Mapping

This document describes the proposed RGB LED effects for various battery states, with colorized labels to match the suggested themes.

<table>
  <tr>
    <th>Battery State</th>
    <th>LED Effect</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><b><span style="color:#7fffd4;">Full & Plugged In</span></b></td>
    <td><b>Aurora</b></td>
    <td>Smooth, flowing aurora effect conveys a fully charged, healthy, and calm state.</td>
  </tr>
  <tr>
    <td><b><span style="color:#1e90ff;">Charging</span></b></td>
    <td><b>Water</b> <i>(occasional <span style="color:#f8f8ff; background:#ffe066;">Lightning</span>)</i></td>
    <td>Water effect represents energy flowing in. Occasional lightning flashes signal active charging or boost events.</td>
  </tr>
  <tr>
    <td><b><span style="color:#00ff00;">In Use, Full</span></b></td>
    <td><b>Green Candle</b> or <b>Lightning</b></td>
    <td>Gentle green candle flicker signals “all good.” Lightning can be used for a more energetic, ready-for-action vibe.</td>
  </tr>
  <tr>
    <td><b><span style="color:#ffa500;">In Use, Mid Power</span></b></td>
    <td><b>Orange Candle</b></td>
    <td>Orange candle flicker suggests caution—still okay, but pay attention. Flicker intensity can increase as battery drops.</td>
  </tr>
  <tr>
    <td><b><span style="color:#ff3333;">Low Battery</span></b></td>
    <td><b>Red Heartbeat</b></td>
    <td>Red, pulsing heartbeat effect signals low battery. Pulse can speed up as battery gets critically low.</td>
  </tr>
</table>

---

**Suggestions:**
- Smoothly transition between effects as battery state changes.
- For “charging,” consider blending water and aurora as the battery nears full.
- For “critical low,” a brief lightning flash before shutdown can serve as a final warning.
- If “plugged in but not charging,” a static aurora or gentle blue glow can be used.

All effects are designed for a single RGB LED. Colorized labels above are for reference and can be tuned to match your actual palette choices.

---

**Note:**
You may also implement a color gradient that tracks battery voltage directly, smoothly shifting the LED color as voltage changes, while still changing the animation style at key current or voltage thresholds. This approach can provide even more intuitive, real-time feedback on battery status.
