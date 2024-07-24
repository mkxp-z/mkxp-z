# Test script for mkxp-z Graphics.transition reported FPS bug fix.
# Run via the "customScript" field in mkxp.json.

puts 'No transition. Counter should be normal'
normal_duration = 2
Graphics.wait(normal_duration * Graphics.frame_rate)

transition_duration = 10 # Default value used in RGSS
num_transitions = 20
puts 'Performing transitions. If the FPS counter notably dips, the bug is not fixed.'
num_transitions.times do
  Graphics.freeze
  Graphics.transition(transition_duration)
  Graphics.wait(transition_duration)
end

exit
