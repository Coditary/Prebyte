{{ if enabled && lua("return upper(name) == 'ADA'") }}ok{{ else }}bad{{ endif }}
