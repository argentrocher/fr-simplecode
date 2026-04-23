<#
.SYNOPSIS
    Associe l'extension .frc à l'exécutable fr-simplecode*.exe trouvé selon les règles spécifiées.
#>

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# Charge l'assembly pour System.Windows.Forms.MessageBox
Add-Type -AssemblyName System.Windows.Forms

# Récupère le chemin du script actuel
$scriptPath = $PSScriptRoot
$currentDir = $scriptPath

# Fonction pour trouver le bon exécutable
function Find-FrSimpleCodeExe {
    param($startDir)

    # Remonte jusqu'à trouver lib ou lib_frc
    $dir = $startDir
    $foundLib = $false
    $foundLibFrc = $false

    while ($dir -ne $null) {
        if ([string]::IsNullOrEmpty($dir)) {
            break
        }

        $parent = Split-Path $dir -Parent
        if ($parent -eq $null) { break }

        $dirName = Split-Path $dir -Leaf
        if ($dirName -eq "lib") {
            $foundLib = $true
            $dir = $parent
            $parentDirName = Split-Path $dir -Leaf
            if ($parentDirName -eq "lib_frc") {
                $foundLibFrc = $true
                $dir = $parent
				$grandParent = Split-Path -Path $dir -Parent
                if ($grandParent -ne $null) {
					$dir = $grandParent
				}
            }
            break
        }
        $dir = $parent
    }
	
	if (-not $foundLib) {
		$dir = $startDir
		$parentDirName = Split-Path $dir -Leaf
        if ($parentDirName -eq "lib_frc") {
            $dir = Split-Path -Path $dir -Parent
			$grandParent = Split-Path $dir -Leaf
            if ($grandParent -ne $null) {
				$foundLibFrc = $true
			}
        }
	}
	
	if (-not $foundLibFrc) {
		$dir = $startDir
	}

    # Cherche l'exécutable fr-simplecode*.exe dans le dossier final
    $exePath = Get-ChildItem -Path $dir -Filter "fr-simplecode*.exe" |
		ForEach-Object {
        # Extrait le nom sans extension
        $baseName = $_.BaseName
        # Extrait la partie après "fr-simplecode"
        $versionPart = $baseName.Substring(13)
        # Nettoie les caractères non numériques
        $cleanVersionPart = $versionPart -replace '[^0-9.]', ''
        # Si aucune version n'est trouvée, on considère "0.0.0"
        if ([string]::IsNullOrEmpty($cleanVersionPart)) {
            $cleanVersionPart = "0.0.0"
        }
        # Ajoute la version nettoyée comme propriété
        $_ | Add-Member -NotePropertyName "CleanVersion" -NotePropertyValue $cleanVersionPart -PassThru
		} |
        Sort-Object { [version]$_.CleanVersion } -Descending |
        Select-Object -First 1 -ExpandProperty FullName

    return $exePath
}

# Trouve l'exécutable
$exePath = Find-FrSimpleCodeExe -startDir $currentDir

if ($exePath) {
    # Définit un type de fichier pour .frc
    $fileType = "FRC_File"
    $extension = ".frc"
	$name_extension = "FRC code"

    try {
        # Crée la clé pour l'extension .frc
        New-Item -Path "HKCU:\Software\Classes\.frc" -Force | Out-Null
        Set-ItemProperty -Path "HKCU:\Software\Classes\.frc" -Name "(Default)" -Value $fileType

        # Crée la clé pour le type de fichier FRC_File
        New-Item -Path "HKCU:\Software\Classes\$fileType" -Force | Out-Null
        Set-ItemProperty -Path "HKCU:\Software\Classes\$fileType" -Name "(Default)" -Value $name_extension
        New-Item -Path "HKCU:\Software\Classes\$fileType\shell\open\command" -Force | Out-Null
        $commandValue = "`"$exePath`" `"%1`""
        Set-ItemProperty -Path "HKCU:\Software\Classes\$fileType\shell\open\command" -Name "(Default)" -Value $commandValue

        [System.Windows.Forms.MessageBox]::Show("L'extension $extension a été définie avec succès sur $exePath.`n`nThe $extension was successfully defined on the $exePath.", "Succès", "OK", "Information")
    }
    catch {
        [System.Windows.Forms.MessageBox]::Show("Erreur lors de l'association de l'extension : $extension sur $exePath`n`nError while associating the extension: $extension on the $exePath", "Erreur", "OK", "Error")
    }
} else {
    [System.Windows.Forms.MessageBox]::Show("Aucun exécutable fr-simplecode*.exe trouvé.`n`nNo executable fr-simplecode*. exe found.", "Erreur", "OK", "Error")
}