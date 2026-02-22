# **Noxiom**

## **What Is Noxiom**

Noxiom is a really lightweight server OS (*operating system*).
Only (***insert amount here***).

## **What Makes Noxiom Different**

Noxiom is made to run servers for beginners or pro developers.
Why you may ask should I use Noxiom? Our reasoning is:  
1. Why not use it, it is your choice  
2. We do it all for you, we:  
    1. Preinstall compilers and interpreters for all common languages  
    2. Preinstall the common languages  
3. It is **not** a pain in the ass to install pip modules (***looking at you Pi OS Lite***)
4. It is cheap to get hardware to run it 
5. We care about your RAM
6. You can either ssh in or use a keybord and the machine it is installed on

## **Hardware Requirements**
No specsifics yet but to my tests it works on a pi 5 or eqevelent and above

## **What Does Noxiom Come Preinstalled With**

Noxiom comes with the languages:
1. Bash/Shell
2. C
3. C#
4. C++
5. JavaScript + Node.js
6. Python
7. HTML

Noxiom comes with the compilers/interpreters for:
1. C/C++ (GCC)
2. Python (The official Python interpreter)

Noxiom comes with the runtimes:
1. .NET for C#
2. Node.js for JavaScript

Noxiom also comes with:
1. Git
2. pip

## **Package Manager**

Noxiom uses pip as its package manager.

## **How To Install Noxiom**

### Easy Install (Recommended)

1. Download **NoxiomInstaller.exe** from the [latest release](https://github.com/sintaxsaint/noxiom/releases)
2. Run it as Administrator
3. Pick your architecture:
   - **arm64** — Raspberry Pi
   - **x86_64** — PC / server
4. Insert your SD card or USB drive
5. Select it from the list (only removable drives are shown — your internal drive is safe)
6. Click **Install Noxiom OS** and wait for it to finish
7. Eject the drive and plug it into your device — done

### Manual Install (for people who want to verify integrity)

Requirements: WSL2 with Ubuntu, nasm, gcc, gcc-aarch64-linux-gnu, qemu

```bash
git clone https://github.com/sintaxsaint/noxiom.git
cd noxiom

# Build for Raspberry Pi
make ARCH=arm64

# Build for PC / server
make ARCH=x86_64
```

Or just double-click **build.bat** on Windows for a menu-driven build.

---

###### yes I used claude to help me