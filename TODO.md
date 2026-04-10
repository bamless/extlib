- Implement tombstone collapsing optimization on deletes when right slot is in EMPTY state, i.e. we
are not breaking any probing chain, so we can mark current slot directly as EMPTY, and collapse any
tombstones to the left of it.
