Diffusion for macOS (Apple Silicon)
(Russian version below)

This archive has the engine and the game libraries for macOS. It does not
include the game data, which comes from Diffusion 1.3.

Requirements
- A Mac with Apple Silicon (M1 or newer)
- macOS 14.8 or newer
- Diffusion 1.3 from https://aynekko.itch.io/diffusion

Install
1. Download Diffusion 1.3 and unpack it. The result is a "Diffusion" folder
   that has a "diffusion" folder inside.
2. Open Terminal and unpack this archive over that folder:

     ditto -x -k diffusion-macos-arm64.zip /path/to/Diffusion

   Don't copy the files in Finder. It offers to replace the whole "diffusion"
   folder, and you would lose the game data.
3. macOS won't start a downloaded unsigned program until you remove the
   quarantine flag:

     xattr -dr com.apple.quarantine /path/to/Diffusion

4. Start the game:

     cd /path/to/Diffusion
     ./diffusion_run

   For Russian, run "./diffusion_run -language russian".
   For Turkish, run "./diffusion_run -language turkish".

Tip: to type a path, drag the file or folder into the Terminal window.



Diffusion для macOS (Apple Silicon)

В архиве движок и игровые библиотеки для macOS. Данных игры в нём нет, они
берутся из Diffusion 1.3.

Требования
- Mac на Apple Silicon (M1 и новее)
- macOS 14.8 или новее
- Diffusion 1.3 с https://aynekko.itch.io/diffusion

Установка
1. Скачайте Diffusion 1.3 и распакуйте. Получится папка "Diffusion", внутри
   которой лежит папка "diffusion".
2. Откройте Терминал и распакуйте этот архив поверх неё:

     ditto -x -k diffusion-macos-arm64.zip /путь/к/Diffusion

   Не копируйте файлы через Finder: он предлагает заменить папку "diffusion"
   целиком, и данные игры пропадут.
3. macOS не запускает скачанные неподписанные программы, пока не снят флаг
   карантина:

     xattr -dr com.apple.quarantine /путь/к/Diffusion

4. Запустите игру:

     cd /путь/к/Diffusion
     ./diffusion_run

   Русский язык: "./diffusion_run -language russian".
   Турецкий язык: "./diffusion_run -language turkish".

Подсказка: чтобы не набирать путь, перетащите файл или папку в окно Терминала.
