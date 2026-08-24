# IWARS Entity Feed

C++20 truth engine + React map editor. Creates / moves entities and publishes them over **UDP**. Blip generation (radar / IFF) happens in the **workplace simulator** — not here.

```
This app (entity truth)  --UDP-->  Workplace radar/IFF app  -->  blips
```

UDP packet layout is a placeholder (`IWP2`) until the real format is provided.

## Run

```bash
IWARS_SCENARIOS=/root/iwars/scenarios /root/iwars/backend/build/iwars_sim
cd frontend && npm run dev
```
