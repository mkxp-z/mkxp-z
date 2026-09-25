# replace_game_script.rb
# Author: zykurv (2024)

# Creative Commons CC0: To the extent possible under law, zykurv has
# waived all copyright and related or neighboring rights to replace_game_script.rb.
# https://creativecommons.org/publicdomain/zero/1.0/

# This can be used to replace any game scripts with custom scripts.
# Your replacement scripts must be added to $RGSS_REPLACE below.
# The game script may be identified either by its index in the
# script list, or by the script name (must match exactly)

# Setting $REPLACE_DEBUG to true will print script information,
# which can be helpful to find a specific script index or verify the name.

$RGSS_REPLACE = {
    # Using script name:
    # "GameScriptFoo" => "scripts/Foo.rb",

    # Using script index:
    # "123" => "scripts/Bar.rb",
}
$REPLACE_DEBUG = false

print("Finding replacement for #{$RGSS_SCRIPTS.size} game scripts...\n") if $REPLACE_DEBUG

for index in 0...$RGSS_SCRIPTS.size
    script_info = $RGSS_SCRIPTS[index]
    script_name = script_info[1]

    print("| ##{index}: \"#{script_name}\"\n") if $REPLACE_DEBUG

    replace_file = nil
    replace_file = $RGSS_REPLACE[index.to_s] if $RGSS_REPLACE[index.to_s] != nil
    replace_file = $RGSS_REPLACE[script_name] if $RGSS_REPLACE[script_name] != nil
    next if replace_file == nil
    
    print(":>  Replace: \"#{replace_file}\"\n") if $REPLACE_DEBUG

    File.open(replace_file, "r") do |file|
        script_info[3] = file.read
    end
end
