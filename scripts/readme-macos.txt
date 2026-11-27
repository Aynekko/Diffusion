Diffusion for macOS
(Russian version below)

This archive has the engine and the game libraries for macOS. It does not
include the game data, which comes from Diffusion game build.

Requirements
- A Mac with Apple Silicon (diffusion-macos-arm64.zip) or an Intel
  processor (diffusion-macos-amd64.zip)
- macOS 14.8 or newer
- Diffusion game build from https://aynekko.itch.io/diffusion

Install
1. Download Diffusion game build and unpack it. The result is a "Diffusion" folder
   that has a "diffusion" folder inside.
2. Open Terminal and unpack this archive over that folder:

     ditto -x -k diffusion-macos-arm64.zip /path/to/Diffusion

   On an Intel Mac, use diffusion-macos-amd64.zip in this command.

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



Diffusion для macOS

В архиве движок и игровые библиотеки для macOS. Данных игры в нём нет, они
берутся из игровой сборки Diffusion.

Требования
- Mac на Apple Silicon (diffusion-macos-arm64.zip) или с процессором
  Intel (diffusion-macos-amd64.zip)
- macOS 14.8 или новее
- Игровая сборка Diffusion с https://aynekko.itch.io/diffusion

Установка
1. Скачайте Diffusion и распакуйте. Получится папка "Diffusion", внутри
   которой лежит папка "diffusion".
2. Откройте Терминал и распакуйте этот архив поверх неё:

     ditto -x -k diffusion-macos-arm64.zip /путь/к/Diffusion

   На Mac с Intel укажите в этой команде diffusion-macos-amd64.zip.

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
