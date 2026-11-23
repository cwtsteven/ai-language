# Model plugins

The same `--model` option accepts GGUF weights or a JSON plugin:

```sh
./build/typed-cangjie task 'Return the sum of 2 and 3.' --model /path/to/model.gguf
./build/typed-cangjie task 'Return the sum of 2 and 3.' --model /path/to/my-plugin.json
```

Models must be supported by the bundled llama.cpp. The runner reads the chat
template and architecture from the GGUF. For reusable memory settings or a
custom prompt format, create a plugin without recompiling:

```json
{
  "name": "My local model",
  "model": "/path/to/model.gguf",
  "gpu_layers": 16,
  "ubatch": 64
}
```

The `model` field is required when selecting a plugin. Relative model paths
are resolved against the plugin file's directory; absolute paths also work.
Set `gpu_layers` to `0` for CPU inference, including CPU builds on macOS.

| Optional field | Meaning |
| --- | --- |
| `name` | Descriptive label; defaults to the file's stem. |
| `gpu_layers` | Layers to offload to the GPU. Defaults to 99 on macOS, 0 elsewhere. |
| `ubatch` | Positive physical batch size. Smaller batches reduce peak prompt memory; defaults to llama.cpp's setting. |
| `chat_template` | A llama.cpp built-in template name, such as `chatml`, or a recognized template. Defaults to the GGUF's template. |
| `prompt_template` | A complete custom prompt containing `{{system}}` and `{{task}}`. Use when the built-in formatter does not support the model's format. |
| `assistant_prefix` | Text or special tokens before generated code. An empty string disables the automatic Qwen3.5/GPT-OSS prefix. |

Choose `chat_template` or `prompt_template`. `typed-cangjie` puts the typed
language specification in `{{system}}` and the request in `{{task}}`.
`plain-llama` uses the same fields for the system and user files it is given;
the benchmark's Cangjie arm supplies the Cangjie excerpt as that system text.
Values are inserted once; placeholders inside them are not evaluated.
Custom templates receive no automatic assistant prefix. See
[plugins/custom.example.json](../plugins/custom.example.json) for a ChatML example.
