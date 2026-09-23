
from PySide6.QtCore import Qt, QUrl, Signal
from PySide6.QtGui import QPixmap, QColor, QFont
from PySide6.QtNetwork import QNetworkAccessManager, QNetworkRequest, QNetworkReply
from PySide6.QtWidgets import (
    QFrame, QVBoxLayout, QLabel, QHBoxLayout, QWidget, QPushButton, QGridLayout,
    QSizePolicy,
)

class Panel(QFrame):
    def __init__(self, title="", parent=None):
        super().__init__(parent)
        self.setObjectName("Panel")
        self.outer = QVBoxLayout(self)
        self.outer.setContentsMargins(9, 6, 9, 8)
        self.outer.setSpacing(5)
        self.title_label = QLabel(title)
        self.title_label.setObjectName("PanelTitle")
        self.outer.addWidget(self.title_label)

class DotLabel(QLabel):
    def __init__(self, color="#86e1ff", diameter=10, parent=None):
        super().__init__(parent)
        self.setFixedSize(diameter, diameter)
        self.setStyleSheet(
            f"background:{color};border:1px solid #c9f6ff;"
            f"border-radius:{diameter//2}px;"
        )

class DataPair(QWidget):
    def __init__(self, key, value="—", key_width=94, parent=None):
        super().__init__(parent)
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(5)
        self.k = QLabel(key)
        self.k.setObjectName("FieldLabel")
        self.k.setFixedWidth(key_width)
        self.v = QLabel(value)
        self.v.setObjectName("Value")
        row.addWidget(self.k)
        row.addWidget(self.v, 1)

class StarterRow(QWidget):
    def __init__(self, key, text, color="#86e1ff", parent=None):
        super().__init__(parent)
        self.key = key
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 1, 0, 1)
        row.setSpacing(7)
        self.dot = DotLabel(color=color, diameter=10)
        self.text = QLabel(text)
        self.text.setObjectName("Value")
        self.button = QPushButton("Select")
        self.button.setObjectName("SmallAction")
        self.button.setMinimumWidth(65)
        row.addWidget(self.dot)
        row.addWidget(self.text, 1)
        row.addWidget(self.button)


class SpriteLoader(QWidget):
    """Small async ORAS sprite cache for party cards.

    Images are presentation-only. Failure to download an image leaves the
    textual party data intact.
    """
    sprite_ready = Signal(int, object)

    def __init__(self, cache_dir, parent=None):
        super().__init__(parent)
        from pathlib import Path
        self.cache_dir = Path(cache_dir)
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.manager = QNetworkAccessManager(self)
        self.manager.finished.connect(self._finished)
        self.pending = {}
        self.download_enabled = True

    def set_download_enabled(self, enabled):
        self.download_enabled = bool(enabled)

    def request_sprite(self, slot, species_id, shiny=False):
        if not species_id:
            self.sprite_ready.emit(slot, QPixmap())
            return

        suffix = "-shiny" if shiny else ""
        path = self.cache_dir / f"{species_id}{suffix}.png"
        if path.exists():
            pix = QPixmap(str(path))
            self.sprite_ready.emit(slot, pix)
            return

        if not self.download_enabled:
            self.sprite_ready.emit(slot, QPixmap())
            return

        # ORAS front sprites from PokeAPI's public sprite repository.
        base = (
            "https://raw.githubusercontent.com/PokeAPI/sprites/master/"
            "sprites/pokemon/versions/generation-vi/"
            "omegaruby-alphasapphire/"
        )
        filename = f"shiny/{species_id}.png" if shiny else f"{species_id}.png"
        reply = self.manager.get(QNetworkRequest(QUrl(base + filename)))
        self.pending[reply] = (slot, path)

    def _finished(self, reply):
        slot, path = self.pending.pop(reply, (-1, None))
        pix = QPixmap()

        if slot >= 0 and reply.error() == QNetworkReply.NoError:
            data = bytes(reply.readAll())
            if data:
                try:
                    path.write_bytes(data)
                except Exception:
                    pass
                pix.loadFromData(data)

        if slot >= 0:
            self.sprite_ready.emit(slot, pix)

        reply.deleteLater()


class PartyCard(QFrame):
    def __init__(self, slot, parent=None):
        super().__init__(parent)
        self.slot = slot
        self.setObjectName("PartyCard")
        self.setMinimumHeight(50)
        self.setMaximumHeight(54)
        self.setMouseTracking(True)

        row = QHBoxLayout(self)
        row.setContentsMargins(6, 4, 6, 4)
        row.setSpacing(6)

        self.image = QLabel("—")
        self.image.setObjectName("PartyImage")
        self.image.setAlignment(Qt.AlignCenter)
        self.image.setFixedSize(40, 40)
        row.addWidget(self.image)

        text = QVBoxLayout()
        text.setSpacing(0)
        self.slot_label = QLabel(f"Slot {slot}")
        self.slot_label.setObjectName("PartySlot")
        self.name_label = QLabel("Empty")
        self.name_label.setObjectName("PartyName")
        self.gender_label = QLabel("—")
        self.gender_label.setObjectName("Muted")
        text.addWidget(self.slot_label)
        text.addWidget(self.name_label)
        text.addWidget(self.gender_label)
        text.addStretch(1)
        row.addLayout(text, 1)

        self.set_party({
            "slot": slot,
            "species": "Empty",
            "species_id": 0,
            "nature": "—",
            "gender": "—",
            "ivs": {},
            "evs": {},
            "hidden_power": "—",
            "pokerus": "—",
            "sv": None,
            "shiny": False,
        })

    @staticmethod
    def _spread(values):
        values = values or {}
        return (
            f"HP {values.get('hp','—')}  /  "
            f"Atk {values.get('attack','—')}  /  "
            f"Def {values.get('defense','—')}  /  "
            f"SpA {values.get('sp_attack','—')}  /  "
            f"SpD {values.get('sp_defense','—')}  /  "
            f"Spe {values.get('speed','—')}"
        )

    def set_party(self, mon):
        species = str(mon.get("species", "Empty"))
        gender = str(mon.get("gender", "—"))
        shiny = bool(mon.get("shiny"))
        evolution = str(mon.get("predicted_evolution") or "").strip()

        self.slot_label.setText(f"Slot {self.slot}")
        self.name_label.setText(("✨ " if shiny else "") + species)
        detail = gender if species != "Empty" else "—"
        if evolution and species != "Empty":
            detail = f"{gender} • {evolution}"
        self.gender_label.setText(detail)

        if species == "Empty":
            self.image.setPixmap(QPixmap())
            self.image.setText("—")
            self.setToolTip(f"<b>Slot {self.slot}</b><br>Empty")
            return

        sv = mon.get("sv")
        sv_text = "—" if sv is None else str(sv)
        tooltip = (
            f"<b>{species}</b> &nbsp; {gender}<br>"
            f"<b>Nature:</b> {mon.get('nature','—')}<br>"
            f"<b>Hidden Power:</b> {mon.get('hidden_power','—')}<br>"
            f"<b>IVs:</b> {self._spread(mon.get('ivs'))}<br>"
            f"<b>EVs:</b> {self._spread(mon.get('evs'))}<br>"
            f"<b>Pokérus:</b> {mon.get('pokerus','—')}<br>"
            + (f"<b>Evolution:</b> {evolution}<br>" if evolution else "")
            + f"<b>SV:</b> {sv_text}"
        )
        self.setToolTip(tooltip)

    def set_sprite(self, pixmap):
        if pixmap is not None and not pixmap.isNull():
            self.image.setText("")
            self.image.setPixmap(
                pixmap.scaled(
                    38, 38,
                    Qt.KeepAspectRatio,
                    Qt.SmoothTransformation,
                )
            )
        else:
            if self.name_label.text() not in ("Empty", ""):
                self.image.setText("—")


class LastSeenFiveSlots(QWidget):
    """Fixed 5-slot Recent encounters view.

    Always paints five rows so the last 5 Pokémon are never clipped by
    QTableWidget geometry. Backend last_seen signals are unchanged.
    """

    SLOT_COUNT = 5
    ROW_HEIGHT = 38

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("LastSeenFiveSlots")
        self._entries = []  # newest first, max SLOT_COUNT
        self._token = 0
        self._sprite_by_token = {}

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Header
        hdr = QWidget()
        hdr.setFixedHeight(26)
        hdr.setObjectName("LastSeenSlotsHeader")
        hl = QHBoxLayout(hdr)
        hl.setContentsMargins(4, 0, 4, 0)
        hl.setSpacing(4)
        for text, stretch, width in (
            ("", 0, 34),
            ("Species", 0, 78),
            ("Nature", 0, 64),
            ("Ability", 1, 0),
            ("HP", 0, 28),
            ("ATK", 0, 28),
            ("DEF", 0, 28),
            ("SPA", 0, 28),
            ("SPD", 0, 28),
            ("SPE", 0, 28),
            ("SUM", 0, 36),
            ("SV", 0, 48),
        ):
            lab = QLabel(text)
            lab.setObjectName("LastSeenSlotsHeaderLabel")
            lab.setAlignment(Qt.AlignCenter)
            if width:
                lab.setFixedWidth(width)
            hl.addWidget(lab, stretch)
        root.addWidget(hdr)

        self._rows = []
        for i in range(self.SLOT_COUNT):
            row = self._make_row()
            root.addWidget(row["frame"])
            self._rows.append(row)

        total_h = 26 + self.SLOT_COUNT * self.ROW_HEIGHT
        self.setFixedHeight(total_h)
        self.setMinimumHeight(total_h)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self._render()

    def _make_row(self):
        frame = QFrame()
        frame.setObjectName("LastSeenSlotRow")
        frame.setFixedHeight(self.ROW_HEIGHT)
        lay = QHBoxLayout(frame)
        lay.setContentsMargins(4, 0, 4, 0)
        lay.setSpacing(4)

        sprite = QLabel()
        sprite.setFixedSize(30, 30)
        sprite.setAlignment(Qt.AlignCenter)
        sprite.setObjectName("LastSeenSlotSprite")
        lay.addWidget(sprite)

        def _lab(width=None, stretch=0, name=""):
            lab = QLabel("—")
            lab.setAlignment(Qt.AlignCenter)
            lab.setObjectName(name or "LastSeenSlotCell")
            if width is not None:
                lab.setFixedWidth(width)
            lay.addWidget(lab, stretch)
            return lab

        return {
            "frame": frame,
            "sprite": sprite,
            "species": _lab(78),
            "nature": _lab(64),
            "ability": _lab(None, 1),
            "hp": _lab(28),
            "atk": _lab(28),
            "defense": _lab(28),
            "spa": _lab(28),
            "spd": _lab(28),
            "spe": _lab(28),
            "sum": _lab(36),
            "sv": _lab(48),
            "token": None,
        }

    @staticmethod
    def _iv_color(value):
        try:
            value = int(value)
        except Exception:
            return "#f2f4f7"
        if value == 31:
            return "#ffd54f"
        if value >= 25:
            return "#4ee07a"
        if value == 0:
            return "#d64cff"
        if value <= 5:
            return "#ff515b"
        return "#f2f4f7"

    def _clear_row(self, row):
        row["token"] = None
        row["sprite"].setPixmap(QPixmap())
        row["sprite"].setText("")
        for key in ("species", "nature", "ability", "hp", "atk", "defense",
                    "spa", "spd", "spe", "sum", "sv"):
            row[key].setText("—")
            row[key].setStyleSheet("")
            row[key].setToolTip("")

    def _fill_row(self, row, data):
        ivs = data.get("ivs") or {}

        def _iv(name):
            try:
                return int(ivs.get(name))
            except Exception:
                return None

        vals = [
            _iv("hp"), _iv("attack"), _iv("defense"),
            _iv("sp_attack"), _iv("sp_defense"), _iv("speed"),
        ]
        total = sum(v for v in vals if v is not None) if all(v is not None for v in vals) else None
        species_name = str(
            data.get("species_name") or data.get("species") or data.get("starter") or "—"
        )
        nature = str(data.get("nature") or "—")
        ability = data.get("ability")
        if ability in (None, "", "—") and data.get("ability_id") is not None:
            try:
                from pokebot.common.ability_names import ability_name
                ability = ability_name(data.get("ability_id"))
            except Exception:
                ability = "—"
        ability = str(ability or "—")
        try:
            sv = int(data.get("shiny_xor"))
        except Exception:
            sv = None
        shiny = bool(data.get("is_shiny"))

        iv_text = "/".join("—" if v is None else str(v) for v in vals)
        sum_text = "—" if total is None else str(total)
        sv_text = "—" if sv is None else f"{sv:,}"
        tooltip = (
            f"{species_name}" + "\n"
            + f"{nature} / {ability}" + "\n"
            + f"IVs: {iv_text}" + "\n"
            + f"Sum: {sum_text}  SV: {sv_text}"
        )

        row["species"].setText(species_name)
        row["nature"].setText(nature)
        row["ability"].setText(ability)
        for key, val in zip(
            ("hp", "atk", "defense", "spa", "spd", "spe"), vals
        ):
            row[key].setText("—" if val is None else str(val))
            if val is not None:
                row[key].setStyleSheet(f"color: {self._iv_color(val)};")
            else:
                row[key].setStyleSheet("")
        row["sum"].setText("—" if total is None else str(total))
        row["sv"].setText("—" if sv is None else f"{sv:,}")
        row["sv"].setStyleSheet(
            "color: #ffd54f; font-weight: 600;" if shiny else "color: #ff5a62;"
        )
        for key in ("species", "nature", "ability", "hp", "atk", "defense",
                    "spa", "spd", "spe", "sum", "sv", "sprite"):
            row[key].setToolTip(tooltip)

        self._token += 1
        token = self._token
        row["token"] = token
        return token, data

    def _render(self):
        for i, row in enumerate(self._rows):
            if i < len(self._entries):
                token, data = self._fill_row(row, self._entries[i])
                # sprite requested by parent via request callback
                row["_pending_species"] = data.get("species_id") or data.get("species")
                row["_pending_shiny"] = bool(data.get("is_shiny"))
            else:
                self._clear_row(row)
                row["_pending_species"] = None
                row["_pending_shiny"] = False

    def set_entries(self, entries):
        """Replace all slots. entries newest-first."""
        self._entries = [dict(e) for e in list(entries or [])[: self.SLOT_COUNT]]
        self._render()
        return self._sprite_requests()

    def prepend(self, data):
        """Insert newest entry at top; drop oldest beyond SLOT_COUNT."""
        self._entries.insert(0, dict(data))
        self._entries = self._entries[: self.SLOT_COUNT]
        self._render()
        return self._sprite_requests()

    def clear(self):
        self._entries = []
        self._render()
        return []

    def _sprite_requests(self):
        """Return list of (token, species_id, shiny) for visible rows."""
        reqs = []
        for row in self._rows:
            token = row.get("token")
            sid = row.get("_pending_species")
            if token is None:
                continue
            try:
                sid = int(sid or 0)
            except Exception:
                sid = 0
            reqs.append((token, sid, bool(row.get("_pending_shiny"))))
        return reqs

    def apply_sprite(self, token, pixmap):
        for row in self._rows:
            if row.get("token") == token and pixmap is not None and not pixmap.isNull():
                scaled = pixmap.scaled(28, 28, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                row["sprite"].setPixmap(scaled)
                return True
        return False
