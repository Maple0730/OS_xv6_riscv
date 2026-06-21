## canvas
- viewBox: 0 0 1280 720
- format: PPT 16:9

## mode
- mode: instructional

## visual_style
- visual_style: swiss-minimal

## colors
- bg: #FFFFFF
- surface: #F4F7FB
- primary: #1E3A8A
- accent: #F59E0B
- accent_bg: #FFF7E8
- secondary_accent: #3B82F6
- text: #111827
- text_secondary: #4B5563
- text_tertiary: #6B7280
- border: #D7E0EC
- grid: #E5EAF2
- success: #059669
- warning: #DC2626

## typography
- font_family: "Microsoft YaHei", "Noto Sans CJK SC", Arial, sans-serif
- code_family: Consolas, "Courier New", monospace
- body: 20
- title: 34
- subtitle: 24
- annotation: 14
- cover_title: 52

## icons
- library: tabler-outline
- stroke_width: 2
- inventory: home-2, route, cpu-2, chart-bar, database, folder-open, eye-off, terminal-2

## images
- p3_system_boot_flow: images/p3_system_boot_flow.png | no-crop
- system_boot_terminal: images/system_boot_terminal.png | no-crop
- p6_memory_overview: images/p6_memory_overview.png | no-crop
- p8_memory_extensions: images/p8_memory_extensions.png | no-crop
- kmalloctest_boot_log: images/kmalloctest_boot_log.png | no-crop
- p10_filesystem_layers: images/p10_filesystem_layers.png | no-crop
- p12_log_and_dualdisk: images/p12_log_and_dualdisk.png | no-crop
- lseektest: images/lseektest.png | no-crop
- hidden_color_ls: images/hidden_color_ls.png | no-crop

## page_rhythm
- P01: anchor
- P02: dense
- P03: dense
- P04: dense
- P05: breathing
- P06: dense
- P07: dense
- P08: dense
- P09: breathing
- P10: dense
- P11: dense
- P12: dense
- P13: dense
- P14: dense
- P15: anchor

## forbidden
- Mixing icon libraries
- rgba()
- <style>, class, <foreignObject>, textPath, @font-face, <animate*>, <script>, <iframe>, <symbol>+<use>
- <g opacity> (set opacity on each child element individually)
- HTML named entities in text (`&nbsp;`, `&mdash;`, `&copy;`, `&ndash;`, `&reg;`, `&hellip;`, `&bull;`)
