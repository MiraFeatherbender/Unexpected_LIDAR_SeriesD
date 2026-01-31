# MCP23017 component — integration notes

This project can include the MCP23017 component from
https://github.com/MiraFeatherbender/MCP23017. The component's sources live in that
repo under `components/mcp23017`.

Recommended approaches

- Submodule (recommended): add the remote repository as a git submodule and place
  it at `components/mcp23017`. This is the simplest for downstream ESP-IDF auto-discovery.

  Add (PowerShell):

  ```powershell
  git submodule add -b main https://github.com/MiraFeatherbender/MCP23017.git components/mcp23017
  git submodule update --init --recursive
  git add .gitmodules components/mcp23017
  git commit -m "Add mcp23017 component as submodule"
  ```

  Update later:

  ```powershell
  git submodule update --remote --merge components/mcp23017
  git add components/mcp23017
  git commit -m "Update mcp23017 submodule"
  ```

- Subtree (alternate): if you need the component's files copied into your repo and want
  to preserve an easy update path without submodules, use `git subtree` to import the
  `components/mcp23017` directory from the remote. Example:

  ```sh
  git remote add mcp https://github.com/MiraFeatherbender/MCP23017.git
  git fetch mcp
  git subtree add --prefix=components/mcp23017 mcp main --squash
  ```

  Update later:

  ```sh
  git fetch mcp
  git subtree pull --prefix=components/mcp23017 mcp main --squash
  ```

Notes

- ESP-IDF auto-discovers components placed under `components/`. No CMake change
  should be required when the component is at `components/mcp23017`.
- If you keep the upstream repo as an entire repo (rather than a root-level component),
  you can also add it as a submodule at another path (for example
  `components/mcp23017-upstream`) and either point CMake at the nested component path
  or copy the inner folder into `components/mcp23017` as a follow-up step.

Using the included scripts

- `scripts/add-mcp23017.ps1` — PowerShell helper to add the submodule and initialize it.
- `scripts/update-mcp23017.ps1` — PowerShell helper to update the submodule pointer and
  commit if there are changes.

Verify build

After adding the component, run `idf.py build` from the project root to ensure the
component is discovered and builds correctly.
