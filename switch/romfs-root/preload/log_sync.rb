# log_sync.rb
# Ruby buffers $stdout/$stderr when they go to a file (sdmc:/mkxpz_log.txt),
# so game output is lost or arrives late if the game hangs or aborts. Write
# it through immediately instead.

$stdout.sync = true
$stderr.sync = true
