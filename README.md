# ARIA — Universal AI for Arduino-compatible USB devices

ARIA is proprietary, closed-source software. Repository access, firmware
library use, and redistribution are controlled by the project owner; no
implied license to copy, modify, or redistribute this code is granted.

ARIA connects a phone or desktop app to Arduino-compatible microcontrollers
over USB serial. Firmware exposes compact actions, variables, and events;
separate `aria.json` or `aria.yaml` metadata describes device capabilities,
units, ranges, and AI guidance. The app supports OpenAI, Google Gemini,
Anthropic Claude, Groq, OpenRouter, Cloudflare AI, and Ollama. Choose a
provider and configure its API key, model, and optional base URL in Settings.

## Architecture

- Mobile app: USB serial transport, metadata editor/import/export, cached
  manifest, and provider-independent AI.
- ARIA firmware library: compact versioned protocol, action and variable
  registration, dispatch, synchronization, and results.
- User sketch: hardware setup and device logic.
- Metadata: portable JSON/YAML, kept out of firmware.
