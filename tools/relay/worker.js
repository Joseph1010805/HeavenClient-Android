/*
 * THE POST BOX THAT IS AWAKE WHEN NOBODY ELSE IS.
 *
 * Two people who are never online at the same moment cannot hand a message
 * to each other, however clever the connection. Something has to HOLD it.
 * That is all this does.
 *
 * It speaks exactly the same three endpoints as the carry port inside the
 * game server (net.server.carry.CarryPort), so the client does not know or
 * care which one it is talking to:
 *
 *     POST /msg/send?to=NAME&from=NAME[&mesos=N]   body = the message
 *     GET  /msg/waiting?to=NAME                    blocks, oldest first
 *     POST /msg/collect?to=NAME                    body = ids, one per line
 *
 * ⚠ NOTHING HERE IS REQUIRED. The game must work with the lights out on the
 * couch - no router, no internet, nothing. This exists only so a message can
 * ALSO reach somebody in another state. If it is unreachable, or was never
 * set up, every local path works exactly as before. That is the rule, and it
 * is the reason the relay is a transport rather than a dependency.
 *
 * WHY CLOUDFLARE: free, no machine to keep running, no operating system to
 * patch, and it is asleep until somebody writes. A handheld in a car cannot
 * rely on a computer at home being switched on.
 *
 * DEPLOYING IT, once, from any PC:
 *
 *     npm install -g wrangler
 *     wrangler login
 *     wrangler kv namespace create MAILBOX
 *     # put the returned id into wrangler.toml
 *     wrangler deploy
 *
 * That prints a URL. Put it in the game's Settings as `RelayURL` and every
 * device using it can write to every other.
 *
 * ⚠ NO ITEMS EVER. Text and mesos only. An item has to leave one database
 * and arrive in another, and a hand-off that half-fails either loses it or
 * makes two of it. Gifts are Duey's job, inside one world.
 */

const MAX_BODY = 4096;      // a message, not a file transfer
const MAX_WAITING = 200;    // per person, so a silent recipient cannot grow forever

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    try {
      if (url.pathname === "/msg/send" && request.method === "POST") {
        return await send(request, url, env);
      }

      if (url.pathname === "/msg/waiting" && request.method === "GET") {
        return await waiting(url, env);
      }

      if (url.pathname === "/msg/collect" && request.method === "POST") {
        return await collect(request, url, env);
      }

      // Same shape of answer as the carry port: a sentence, not a page.
      if (url.pathname === "/msg/health") {
        return text("ok RELAY 1\n");
      }

      return text("no such thing here\n", 404);
    } catch (err) {
      // THE REASON GOES BACK. The thing at the other end is a handheld with
      // no console and a child holding it.
      return text("relay failed: " + err + "\n", 500);
    }
  },
};

function text(body, status = 200) {
  return new Response(body, {
    status,
    headers: { "content-type": "text/plain; charset=utf-8" },
  });
}

/** Everything for one person, as a list we can rewrite. */
async function load(env, who) {
  const raw = await env.MAILBOX.get("box:" + who);

  return raw ? JSON.parse(raw) : [];
}

async function save(env, who, box) {
  await env.MAILBOX.put("box:" + who, JSON.stringify(box));
}

async function send(request, url, env) {
  const to = url.searchParams.get("to");
  const from = url.searchParams.get("from");

  if (!to || !from) {
    return text("who to, and who from?\n", 400);
  }

  const body = await request.text();

  if (body.length > MAX_BODY) {
    return text("that message is too long\n", 413);
  }

  const box = await load(env, to);

  if (box.length >= MAX_WAITING) {
    // Refused rather than silently dropped: the sender is told, and can try
    // again once the other person has actually read something.
    return text("that mailbox is full\n", 507);
  }

  // The id has to be unique per mailbox and to survive the worker having no
  // memory between requests, so it counts from what is already there.
  const id = box.length ? box[box.length - 1].id + 1 : 1;

  box.push({
    id,
    sender: from,
    body,
    mesos: Math.max(0, parseInt(url.searchParams.get("mesos") || "0", 10) || 0),
    sent: Date.now(),
  });

  await save(env, to, box);

  return text("sent " + id + "\n");
}

async function waiting(url, env) {
  const to = url.searchParams.get("to");

  if (!to) {
    return text("waiting for whom?\n", 400);
  }

  const box = await load(env, to);

  // BYTE FOR BYTE THE CARRY PORT'S FORMAT. The body is length-counted and
  // comes last, so a message may contain tabs, newlines, anything at all.
  let out = "";

  for (const m of box) {
    out += `MSG\t${m.id}\t${m.sender}\t${m.mesos}\t${m.sent}\t${m.body.length}\n${m.body}\n`;
  }

  return text(out);
}

async function collect(request, url, env) {
  const to = url.searchParams.get("to");

  if (!to) {
    return text("collected by whom?\n", 400);
  }

  const wanted = new Set(
    (await request.text())
      .split("\n")
      .map((line) => parseInt(line.trim(), 10))
      .filter((n) => !isNaN(n))
  );

  const box = await load(env, to);
  const keep = box.filter((m) => !wanted.has(m.id));

  await save(env, to, keep);

  // A SECOND STEP ON PURPOSE, exactly as the carry port does it: reading does
  // not delete. A handheld that receives a message and loses power before
  // writing it down would otherwise lose it for good.
  return text("collected " + (box.length - keep.length) + "\n");
}
