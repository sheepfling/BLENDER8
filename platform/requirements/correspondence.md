# Blender-8: the customer correspondence and parts file

**From:** Rick / Half-A/Labs  
**To:** Firmware team / Half-A/Labs  
**Date:** 2026-10-06T16:42:00-05:00  
**Subject:** Blender-8: the customer correspondence and parts file

Team,

I went back through the file for Mara's answers. These are the copies I could find, including my
clarification sketches and the revised r4 chassis drawings. Some have been through the copier
more than once. The supplier papers are behind them.

Read each exchange through to Mara's last reply. My last message in each thread is the
interpretation she accepted; the earlier questions explain why we landed there. There is not a
separate requirements table to hunt for.

The jug interlock came up late. Read that last exchange before using the drawings: the enclosed
r4 sheets replace the older r3 attachments mentioned in September. The new STOP acknowledgment
also applies at startup. We are sensing the jug seating, not the lid.

We can build with either Northstar MCU: B8 is the byte-integer option; B16 adds word integers,
binary32 float, DMA and selectable peripheral pins. Neither profile permits doubles or wide
integer scalars. The B8 manual is the shared peripheral reference; use the B16 follow-on with it
for that device. We will use the same customer decisions with either choice. The B16 bench
connects motor PWM to PB5, tach to PB1, LCD VBLANK to PB6 and the temperature input to PA4/AN0;
we must configure and lock those routes before using them. Please build against the public SDK,
with no private bench readings or workstation clock.

The developer route has the install and PDF commands. Select B8 or B16 with --device when
building native tests or WebAssembly; each gets its own build folder. We can load the compiled
.wasm and its matching .mjs into the local workbench page, restart from a clean instance,
exercise the controls and save a test result and journal. Bring back the source, device choice,
local build/test command, clock plan and traces of the awkward cases. The page's execution check
is a first check; we still need to test Mara's accepted behavior. The part manuals describe the
hardware; these emails are the customer decisions.

The scan strategy, debounce implementation, tach estimator and task layout are yours. Do not
turn a sketch into a demanded software architecture. Flag anything the correspondence does not
settle before you silently choose a customer-facing behavior. We can ask Mara.

Rick  
Half-A/Labs

---

## Can we keep the front panel simple?

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-08T09:14:00-05:00  
**Subject:** [EXTERNAL] Can we keep the front panel simple?

Rick, I want seven speeds that feel evenly spaced, plus a bit more while you hold PULSE. Nothing
fancy on the little screen. Just enough that someone can tell what they pressed.

We have the button assembly and motor papers. Can your team work from those rather than us
changing the panel again?

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-08T11:02:00-05:00  
**Subject:** RE: Can we keep the front panel simple?

Yes. Before we settle the behavior: evenly spaced shaft speeds, or evenly spaced power settings?
Those are not the same on this motor.

I have sketched PULSE as a temporary boost on top of a latched selection. Is that what you
meant, rather than repeatedly switching the motor on and off? What should it do when no speed is
selected?

For the screen, is the number the selected position or measured RPM?

[Sketch: PULSE boost and return to the selected speed; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-08T14:26:00-05:00  
**Subject:** [EXTERNAL] RE: Can we keep the front panel simple?

Evenly spaced speeds. It can slow down when the jug is loaded; I do not need it fighting to hold
the speed.

Your boost sketch is right. Let PULSE run gently on its own, too. The screen should show the
selection, not a speedometer. A number and a P would be enough.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-09T10:18:00-05:00  
**Subject:** RE: Can we keep the front panel simple?

Then we'll use 3,000 to 18,000 RPM in seven equal steps. PULSE adds 2,000 RPM while held, capped
at 20,000; from idle it asks for 2,000. Release returns to the selected speed or idle.

At nominal no load, we'll allow one second to settle and an error of 100 RPM or 2% of target,
whichever is larger. Loaded sag is acceptable; speed regulation is not part of this job.

The display will show 0, 1 through 7, P on its own, or 1P through 7P. Stable button changes
should take effect within 40 ms after the contacts settle. Taps shorter than 50 ms need not
register. We'll choose how to scan and debounce the contacts; a brief changeover must not pick
an arbitrary speed. Two speed indications persisting for 40 ms will stop the drive and show IF.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-09T11:07:00-05:00  
**Subject:** [EXTERNAL] RE: Can we keep the front panel simple?

Yes, that is the panel behavior I meant. Please use those numbers. No speedometer or regulation
loop.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## STOP is not the rear switch

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-10T08:42:00-05:00  
**Subject:** [EXTERNAL] STOP is not the rear switch

I keep coming back to the stop behavior. STOP should stop it, and switching it off and on should
not surprise anyone. The buttons stay down when the rear switch is off. Is that going to be a
problem?

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-10T10:31:00-05:00  
**Subject:** RE: STOP is not the rear switch

Only if we treat a retained button as a new request. The panel already removes a separate
hardware permission when STOP is pressed. We'll have the firmware remove its own requests as
well.

By "stop", do you mean remove electrical drive promptly, or brake the shaft to a standstill? The
supplied motor will coast. Also, should releasing STOP ever resume a held speed or PULSE?

[Sketch: STOP release and a fresh command; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-10T13:15:00-05:00  
**Subject:** [EXTERNAL] RE: STOP is not the rear switch

Remove drive. We are not adding a brake. Releasing STOP must not start anything, including a
PULSE somebody kept holding.

After a power interruption, I want the operator to let the controls go and make a fresh
selection. Do not remember what they were doing and carry on.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-11T09:06:00-05:00  
**Subject:** RE: STOP is not the rear switch

Agreed. From STOP assertion, the firmware has 10 ms to remove its enable request, disable PWM
and clear the pending drive request, even when the independent gate has already done its job. We
will measure that at the outputs, not wait for zero shaft speed.

STOP release does not restore anything. A PULSE held through STOP must be released and pressed
again. After any power-up or controller reset, we'll keep drive off until the startup
temperature check is complete, STOP is released and every command has been released. Only a
later new command may run it.

We'll choose the debounce algorithm and release-counter size, and demonstrate that bounce,
retained buttons and switch interruptions do not create a new command. The hot/faulted restart
sequence needs its own agreement; I will send that separately.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-11T09:48:00-05:00  
**Subject:** [EXTERNAL] RE: STOP is not the rear switch

Yes. That distinction between removing drive and the shaft coasting is fine. Nothing should
resume merely because STOP or power comes back.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## It should know when it gets stuck

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-15T09:21:00-05:00  
**Subject:** [EXTERNAL] It should know when it gets stuck

Could it switch itself off if something wedges? I would rather not keep pouring power into it
until it gets hot. But I do not want nuisance stops every time it starts with a heavy jug.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-15T11:46:00-05:00  
**Subject:** RE: It should know when it gets stuck

The tach output lets us check whether motion follows a drive request. It cannot tell a blocked
shaft from a broken tach lead; I suggest we stop for either and call it unverified motion.

How about allowing ordinary run-up for the first half-second, then requiring a definite shutdown
if motion never appears? For a motor that was already running, I suggest a tighter limit. No
automatic kick or retry after an obstruction clears. Does that match your expectation?

[Sketch: Run-up and missing-motion checks; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-15T15:08:00-05:00  
**Subject:** [EXTERNAL] RE: It should know when it gets stuck

Yes. A feedback problem is a reason to stop, not to guess. Half a second for starting sounds
reasonable.

Once it is running, catch a jam in less than half a second. A failed start should be off well
inside a second. No kicks and no automatic retry; somebody needs to deal with it.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-16T09:33:00-05:00  
**Subject:** RE: It should know when it gets stuck

We'll use 750 ms from a run command to drive removal for a fully blocked start, and 400 ms from
a sudden blockage once running. We'll allow the normal 500 ms run-up interval only when drive
actually changes from off to on; changing speed or adding PULSE must not buy another grace
period.

We'll choose the tach estimator and no-motion qualification so that normal run-up and permitted
loaded operation pass, but those two deadlines do not slip. We are not prescribing a rolling
window or pulse-count recipe.

We'll show ST and keep the fault latched until deliberate recovery, without waiting for heat
before stopping a cold jam. Intentional drive-off and coastdown are not new stalls. A brief
PULSE that ends before a motion decision simply ends; it must not schedule a restart or retry.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-16T10:12:00-05:00  
**Subject:** [EXTERNAL] RE: It should know when it gets stuck

That works. Use 750 ms and 400 ms. ST can cover missing feedback as well as a jam; I care that
it stops rather than pretending it knows the cause.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## Too hot, or just a bad temperature reading?

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-17T08:56:00-05:00  
**Subject:** [EXTERNAL] Too hot, or just a bad temperature reading?

We should stop it if it gets too hot, even if it is still turning. Once it cools, can the user
just press a button? And what happens if the temperature lead comes loose?

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-17T10:40:00-05:00  
**Subject:** RE: Too hot, or just a bad temperature reading?

I would keep overheating separate from a bad signal. The attached case sensor lags the case; it
does not measure a winding hotspot. Its voltage paper is in the motor packet.

For this build, may I use 85 degrees C for shutdown and 65 degrees C for recovery, with a short
cool/stable check? I would not let cooling alone restart it. My sketch requires a new STOP
acknowledgment after the check, then released controls and a new selection.

An open lead on our board is pulled high. I propose TS for an implausible or stale temperature
channel, rather than treating every rail reading as proof of overheating.

[Sketch: Cooling and deliberate recovery; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-17T14:04:00-05:00  
**Subject:** [EXTERNAL] RE: Too hot, or just a bad temperature reading?

Yes, use those two temperatures for this build. When it trips, turn it off rather than running
slowly.

I like the separate signal-fault indication. Keep checking temperature while it is stopped. The
STOP press needs to happen after it is ready to recover; holding STOP while it cools should not
count.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-18T09:17:00-05:00  
**Subject:** RE: Too hot, or just a bad temperature reading?

Then we'll take fresh temperature readings at least every 10 ms. Two successive plausible
readings at or above 85 degrees C qualify TH; we'll remove drive within 10 ms of that decision.
A disconnected, grounded or railed lead, or more than 100 ms without a fresh completed reading,
qualifies TS and gets the same shutdown response. We'll choose and justify the analog
plausibility band from the supplied parts; a constant believable reading alone is not proof of a
fault.

At startup, and before recovery from a fault, we'll require fresh plausible readings at or below
65 degrees C continuously for two seconds. After a fault, we'll accept a new STOP press only
then. STOP and all commands must be released before a later new selection can run. Cooling,
repaired wiring or cleared obstruction alone never releases the lockout.

We'll show a qualified fault within 100 ms, without delaying shutdown, and keep every
independently detected cause; for these faults we'll display TS before TH, then IF, then ST.
These are our product limits, not motor or sensor ratings.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-18T10:03:00-05:00  
**Subject:** [EXTERNAL] RE: Too hot, or just a bad temperature reading?

Yes, please proceed on that basis. Two seconds cool, then a fresh STOP and a new command after
everything is released. No queued restart.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## Which clock are we actually using?

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-23T09:08:00-05:00  
**Subject:** [EXTERNAL] Which clock are we actually using?

I found the oscillator sheet in the parts file. I thought that was there so the timings would
stay consistent. Can we use it rather than relying on the internal clock?

Also, does the rear switch really turn everything off together? I do not want a software delay
hiding a power problem.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-23T12:12:00-05:00  
**Subject:** RE: Which clock are we actually using?

Yes. The XO8-33 is an 8 MHz powered oscillator, not a bare crystal. I have attached the current
bench-harness and rear-switch drawings. Its output goes to B8 CLKIN; the display does not share
that clock.

We'll choose the PLL/divider settings. I suggest allowing an 8-32 MHz system clock but fixing
the peripheral clock at 1.000 MHz, within 0.1%, so the existing timing envelope stays useful.

The rear switch feeds separate motor and logic branches. The enable gate also needs qualified
logic power and reset release: the motor rail can still be live when logic browns out. Please
confirm this is the fixed DC bench chassis, not a request for a mains or PCB redesign.

[Sketch: Clock qualification before drive; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-23T15:19:00-05:00  
**Subject:** [EXTERNAL] RE: Which clock are we actually using?

Correct: use that chassis and the oscillator. No mains work or board redesign in this order.

Your team can choose the multipliers. If the clock is missing, stay stopped and tell us. Do not
fall back to a timing guess and keep blending. The drawings are the connections I expect you to
use.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-24T10:25:00-05:00  
**Subject:** RE: Which clock are we actually using?

Agreed: external PLL for normal drive, SYSCLK 8-32 MHz and PBCLK 1.000 MHz within 0.1%. That
gives a nominal 3,906.25 Hz PWM carrier. We'll supply a clock calculation and timing margins; no
prescribed divider tuple.

With a healthy oscillator, we'll verify the active run clock within 100 ms of first reset-entry
execution. All drive requests stay off during power settling, reset and clock switching. A
missing or unqualified source stays off and shows CK; internal startup clock is allowed only for
startup and fault handling. Loss of the active external source must inhibit drive within 1 ms,
with no automatic restart when it returns.

The two attached drawings fix the power, reset, signal and clock connections. Component-local
timings stay local. Fuse positions are shown, but fuse ratings and PCB detail are not released;
those need separate hardware work.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-24T11:16:00-05:00  
**Subject:** [EXTERNAL] RE: Which clock are we actually using?

Confirmed. Use those clock limits and the two drawings. Keep the hardware-rating work separate.
A recovered clock is not permission to start again.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## Please do not let it sit there dead

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-28T08:38:00-05:00  
**Subject:** [EXTERNAL] Please do not let it sit there dead

One last thing: I have seen a screen keep looking fine while the controls stopped responding.
Can we use the watchdog? There is also a deadman timer in that controller manual. Are we
expecting another switch for that?

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-28T10:14:00-05:00  
**Subject:** RE: Please do not let it sit there dead

No additional operator switch. Here the deadman is the controller's windowed execution timer.
The independent watchdog covers elapsed time; the deadman adds a service window while the core
is clocking.

We'll tie timer renewal to fresh input work, a completed temperature acquisition, a protection
decision and the outputs being applied. A ticking interrupt alone will not authorize renewal.

Can we use a worst-case watchdog timeout of 250 ms, and a deadman expiry no longer than 100 ms
at the slowest approved run clock? We can bring up the watchdog first and add the deadman window
before final delivery.

[Sketch: Useful progress before timer renewal; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-28T13:51:00-05:00  
**Subject:** [EXTERNAL] RE: Please do not let it sit there dead

Yes, both for delivery, and no extra switch. If it resets because it lost its way, I want it to
come back stopped with a reason, not resume the old setting.

Please show me a case where the timer is still ticking but the useful work has stopped. That is
the failure I am worried about.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-09-29T09:32:00-05:00  
**Subject:** RE: Please do not let it sit there dead

Agreed. We'll configure and lock both monitors before allowing drive, with at most 250 ms for
the slowest watchdog timeout and 100 ms to deadman expiry at the slowest approved run clock.
We'll choose the legal periods, service window and progress checks; a stale completion cannot
authorize renewal twice. Normal STOP and protection deadlines still apply.

After watchdog or deadman reset, we'll show WD or DM; clock trouble shows CK. We'll capture the
reset causes before clearing them. These indications take priority CK, WD, DM, TS, TH, IF, ST;
we'll keep subordinate causes. We'll show the reset diagnostic within 100 ms after reset
release, then follow the same deliberate recovery we agreed for other faults. A warm reset must
not assume the display, button latches, shaft or temperature were reset too.

We will demonstrate lost foreground progress with a live tick, missing clock, and brownout with
the motor rail energized. A healthy-looking screen or a reset by itself is not proof of safe
recovery.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-09-29T10:11:00-05:00  
**Subject:** [EXTERNAL] RE: Please do not let it sit there dead

Yes, that closes it for me. Please have the team go ahead on that basis. Those demonstrations
will be useful at the review.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

## One thing we missed: the jug can come off

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-10-01T09:08:00-05:00  
**Subject:** [EXTERNAL] One thing we missed: the jug can come off

Rick, one thing we missed: the jug can be lifted off the base. It must not run unless that piece
is properly engaged. I assumed the safety switch would take care of it. Is it on your drawing?

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-10-01T11:16:00-05:00  
**Subject:** RE: One thing we missed: the jug can come off

Not yet. Do you mean the jug fully seated and locked, rather than the lid? And if someone lifts
it while running, should putting it back resume the selected speed?

I would put the contact in the hardware permission path, with a dropout latch so even a brief
opening cannot re-enable the drive just because the contact closes again. The B8 also gets
status inputs. Updated drawings are attached; these replace the two r3 sheets from September.

Cutting drive is not a brake. We still need a separate mechanical review of access to the
coasting coupling and blades.

[Sketch: Jug permission and deliberate restart; retained in the PDF edition.]

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-10-01T14:22:00-05:00  
**Subject:** [EXTERNAL] RE: One thing we missed: the jug can come off

The jug, seated and locked. We are not adding a lid sensor in this build. Please do not let
reseating it resume anything, including PULSE.

Use STOP to reset the permission, then make the operator choose a speed again. It is fine to
show IL while it is waiting. Keep the mechanical access review separate; I am not calling this
approval of the blade guarding.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---

**From:** Rick / Half-A/Labs  
**To:** Mara Ellis / Kestrel Appliance Systems  
**Date:** 2026-10-02T09:41:00-05:00  
**Subject:** RE: One thing we missed: the jug can come off

Agreed. An open contact or broken interlock lead removes hardware drive permission within 1 ms,
without firmware. It stays removed until the jug is closed steadily for 20 ms and a new STOP
press occurs. Holding STOP while fitting the jug does not count. Power loss or controller reset
also drops that latch.

Our firmware will clear its enable, PWM enable and pending request within 10 ms of the interlock
trip. We'll show IL within 100 ms unless a higher-priority fault is present. The full priority
is CK, WD, DM, TS, TH, IF, IL, ST. We won't create ST merely because the interlock stopped the
drive.

At startup or after opening, we'll require the jug, our existing cooling checks, a new STOP
acknowledgment, released controls and then a fresh speed or PULSE command. Reseating alone never
starts it. We'll demonstrate that sequence with a retained speed, held PULSE, and a quick
contact interruption. The single contact cannot prove a welded or bypassed contact is healthy.

Rick  
Half-A/Labs

---

**From:** Mara Ellis / Kestrel Appliance Systems  
**To:** Rick / Half-A/Labs  
**Date:** 2026-10-02T10:05:00-05:00  
**Subject:** [EXTERNAL] RE: One thing we missed: the jug can come off

Yes. Use the r4 drawings and that sequence. Please put this exchange in the team's file; I do
not want the old drawing to win just because someone printed it first.

![Kestrel K logo](assets/kestrel-mark.svg) Mara Ellis  
Kestrel Appliance Systems | <mara.ellis@kestrel.example>

---
