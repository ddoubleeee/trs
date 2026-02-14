# trs

Lightweight tool for interactive transliteration (or other key=value mappings) with clipboard support


## Preview

![Demo](assets/demo.gif)


## Description

####  `trs` allows you to define a set of `key=value` pairs and interactively translate typed keys to corresponding values, automatically copying final output to the clipboard.

`trs` started as a tiny personal script that allowed me to message in non-Latin languages without having an extra keyboard layout. It can be useful for others who rely on transliteration, though the `key=value` logic makes it handy for any other mappings as well.


## Building

Clone the repo:

```bash
git clone "https://github.com/ddoubleeee/trs"
cd trs
```


Build:

```bash
make
```

Install the binary (`/usr/local/bin/`) and create an example dictionary at `$HOME/.config/trs/example.dict`:

```bash
make install
```

Clean after install:

```bash
make clean
```


### Dependencies

* Unix-like OS (Linux, BSD, MacOS)

* Standard C library

  *Optional (clipboard support):*

  - Wayland: `wl-clipboard`
  - X11: `xclip`


## Running

To run:

```bash
trs
```

After running, you can enter a string and press `Space` for `trs` to replace what you typed in the terminal with the translation.  Characters with no defined translation are left unchanged. It is possible to edit entered but not yet translated word using `Backspace`. You can keep inputting and translating to concatenate, or exit with `Enter` which will display the final result of the translation and copy it into the clipboard *(if supported; see Dependencies)*.


## Configuration

`trs` will choose the first in alpha-numeric order `*.dict` file to use as a dictionary at `$HOME/.config/trs/`.
`*.dict` file should contain a list of pairs of form `KEY=VALUE`.
`trs` will translate `VALUE` to `KEY` (`' '` and `'\n'` characters cannot be overwritten due to `Space` and `Enter` behaviour)

You can use transliteration dictionaries provided in `dicts/` for reference when creating your own ones.