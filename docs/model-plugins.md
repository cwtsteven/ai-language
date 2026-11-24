# Model plugins

`--model` accepts GGUF weights or a JSON plugin:

```sh
./build/typed-cangjie generate 'Return the sum of 2 and 3.' --model /path/to/model.gguf
./build/typed-cangjie generate 'Return the sum of 2 and 3.' --model /path/to/my-plugin.json
```

The model must be one the bundled llama.cpp can load. The runner reads the chat template and architecture from the GGUF. A plugin sets memory and prompt options without recompiling:

```json
{
  "name": "My local model",
  "model": "/path/to/model.gguf",
  "gpu_layers": 16,
  "ubatch": 64
}
```

`model` is required. A relative path is resolved from the plugin file's directory. `gpu_layers: 0` forces CPU inference, including on macOS.

| Optional field | Meaning |
| --- | --- |
| `name` | Label. Defaults to the file stem. |
| `gpu_layers` | GPU layers. Default 99 on macOS, 0 elsewhere. |
| `ubatch` | Positive physical batch size. Smaller values lower peak prompt memory. Default is llama.cpp's. |
| `chat_template` | A llama.cpp template name, such as `chatml`. Default is the GGUF template. |
| `prompt_template` | A full prompt containing `{{system}}` and `{{task}}`, for a format the built-in templates do not cover. |
| `assistant_prefix` | Text inserted before generated code. An empty string disables the automatic Qwen3.5/GPT-OSS prefix. |

Use `chat_template` or `prompt_template`, not both. `typed-cangjie` puts the language specification in `{{system}}` and the request in `{{task}}`. `plain-llama` uses the same fields; the benchmark's Cangjie arm supplies the Cangjie excerpt as the system text. Each value is inserted once. A custom template gets no automatic assistant prefix. See [plugins/custom.example.json](../plugins/custom.example.json).
