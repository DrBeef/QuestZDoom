QuestZDoom
==========

Welcome to the VR port of the popular LZDoom/GZDoom engine for the Oculus Quest.

This is built solely for the Oculus Quest VR HMD and will *not* run on any other device.

The easiest way to install this on your Quest is using SideQuest, a Desktop app designed to simplify sideloading apps and games ( even beat saber songs on quest ) on Standalone Android Headsets like Oculus Quest and Oculus Go. It supports drag and drop for installing APK files!

Download SideQuest here:
https://github.com/the-expanse/SideQuest/releases



IMPORTANT NOTE
--------------

This is just an engine port; though the apk does contain the shareware version of Doom, not the full game. If you wish to play the full game you must purchase it yourself, steam is most straightforward:  https://store.steampowered.com/app/2280/Ultimate_Doom/


Copying the Full Game WAD files to your Oculus Quest
----------------------------------------------------

Copy the WAD files from the installed Doom game folder on your PC to the /QuestZDoom/wads folder on your Oculus Quest when it is connected to the PC. You have to have run QuestZDoom at least once for the folder to be created and if you don't see it when you connect your Quest to the PC you might have to restart the Quest.



Caveats
-------

WARNING:  There is a good chance that unless you have your VR-legs this will probably make you feel  sick. The moment you start to feel under the weather YOU MUST STOP PLAYING for a good period of time before you try again. I will not be held responsible for anyone making themselves ill.

I have included a teleport mechanism for locomotiom, so I would recommend trying that if you do start to feel unwell.


Controls
--------

All these controls are for right-handed mode and are the basic/default mappings, all button pushes map internally to a key press and they can be easily remapped to any function in the options menu. Furthermore, each button has a secondary mode when the dominant hand grip trigger is pushed. So for example in right-hand mode the A button will open doors (by sending a Space key) but with the grip trigger held down it will send a different key to the game instead (which can be remapped as desired)


* Open the in-game menu with the left-controller menu button (same irrespective of right/left handed control)

*Primary Button Functions*

* A Button - Open Door / Switch
* B Button - Jump
* Y Button - 
* X Button - 

* Dominant-Hand Controller - Weapon orientation
* Dominant-Hand Thumbstick - left/right Snap turn, up/down weapon change
* Dominant-Hand Thumbstick click - 
* Dominant-Hand Trigger - Fire Weapon
* Dominant Grip Button - When held down, secondary button functions are available

* Off-Hand Controller - Direction of movement (or if configured settings HMD direction is used)
* Off-Hand Thumbstick - locomotion / teleport
* Off-Hand Trigger - Run
* Off-Hand Grip Button - Weapon Stabilisation - two handed weapon mode
* Off-Hand Thumbstick click - 

*Secondary Button Functions*

Accessed by holding down the dominant hand grip button - All these can be reassigned in the options menu

* A Button - 
* B Button - 
* Y Button - 
* X Button - 

* Dominant-Hand Thumbstick click - 
* Dominant-Hand Trigger - Secondary Fire Weapon

* Off-Hand Trigger - 
* Off-Hand Thumbstick click - 


Things to note / FAQs:
----------------------

* Mods and Texture packs work as per GZDoom
* I won't be implementing a laser sight
* I won't be implementing a vignette comfort mask for locomotion, that is what the teleport is for


Mods:
-----

In order to use mods, you need to supply command line params. For example to play Brutal Doom with the 3D weapon model pack:

'''
qzdoom
'''


Recommendations:
----------------

* Play Brutal Doom
* Use the excellent weapon model pack from  which can be found here:


Known Issues:
-------------

_Performance_: Vanilla unmodded Doom is fine, however once you start using mods then the GZDoom engine is quite resource hungry. Brutal Doom is great fun and plays pretty well, but performance is mediocre at best, so you have been warned. Expect significant framedrops when there is a lot going on, this is unfortunately just a result of the system requirements of the engine.


Credits:
--------

I would like to thank the following teams and individual for making this possible:

* Emile Belanger - For being happy and helpful regarding using his Android build of LZDoom as a basis for this. See his other Android ports [here](http://www.beloko.com/)
* [The ZDoom Teams](https://zdoom.org/index) - For the excellent engine this based upon.
* FishBiter and anyone involved in the OpenVR GZDoom project, from which a lot of the functionality for this project was taken or used as inspiration
* Baggyg - My long-time VR friend whose roles in this have been varied and all helpful, also the creator of excellent websites/artwork/assets for this mod as well as altering models to be more VR friendly
* The [SideQuest](https://sidequestvr.com/#/news) team - For making it easy for people to install this

