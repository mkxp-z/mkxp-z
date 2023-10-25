module Audio
  def self.setup_midi
    raise NotImplementedError
  end

  def self.bgm_play(filename, volume = nil, pitch = nil, pos = nil)
    raise NotImplementedError
  end

  def self.bgm_stop
    raise NotImplementedError
  end

  def self.bgm_fade(time)
    raise NotImplementedError
  end

  def self.bgm_pos
    raise NotImplementedError
  end

  def self.bgm_volume(track = nil)
    raise NotImplementedError
  end

  def self.bgm_set_volume(vol, track = nil)
    raise NotImplementedError
  end

  def self.bgs_play(filename, volume = nil, pitch = nil, pos = nil)
    raise NotImplementedError
  end

  def self.bgs_stop
    raise NotImplementedError
  end

  def self.bgs_fade(time)
    raise NotImplementedError
  end

  def self.bgs_pos
    raise NotImplementedError
  end

  def self.me_play(filename, volume = nil, pitch = nil)
    raise NotImplementedError
  end

  def self.me_stop
    raise NotImplementedError
  end

  def self.me_fade(time)
    raise NotImplementedError
  end

  def self.se_play(filename, volume = nil, pitch = nil)
    raise NotImplementedError
  end

  def self.se_stop
    raise NotImplementedError
  end

end
