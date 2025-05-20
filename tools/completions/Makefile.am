PREFIX=/usr/share
EXTRA_DIST = \
	fish-completion.fish \
	zsh-completion.zsh

.PHONY: install-zsh-completion
install-zsh-completion:
	install -Dm644 zsh-completion.zsh $(PREFIX)/zsh/site-functions/_chafa

.PHONY: install-fish-completion
install-fish-completion:
	install -Dm644 fish-completion.fish $(PREFIX)/fish/vendor_completions.d/chafa.fish
